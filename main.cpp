#include <bits/stdc++.h>
using namespace std;

struct Server {
    int id = 0;
    int gpu = 0;
    int gpuMem = 0;
    int cpu = 0;
    int mem = 0;
};

struct Task {
    int id = 0;
    int r = 0;
    int p = 0;
    int g = 0;
    int v = 0;
    int c = 0;
    int m = 0;
    int w = 0;
};

struct RunningJob {
    int taskId = 0;
    int start = 0;
    int finish = 0;
    int gpu = 0;
    int cpu = 0;
    int mem = 0;
    int gpuMemNeed = 0;
};

struct Answer {
    int taskId = 0;
    int serverId = -1;
    int start = 0;
    int useGpu = 0;
    int finish = 0;
};

struct Metrics {
    long long wait = 0;
    long double idleMem = 0;
    int finish = 0;
};

struct Plan {
    vector<Answer> ans;
    Metrics metrics;
};

struct Weight {
    long long waitW;
    long long finishW;
    long long gpuWasteW;
    long long resourceW;
    int mode;
};

int M, N;
vector<Server> servers;
vector<Task> tasks;
vector<vector<int>> minGpu;
vector<vector<char>> feasible;

long long resourceNeed(int i) {
    const Task& t = tasks[i];
    return 1000000LL * t.g + 3000LL * t.v + 80LL * t.c + t.m;
}

int calcMinGpu(const Task& task, const Server& server) {
    int gpuByMem = (task.v + server.gpuMem - 1) / server.gpuMem;
    return max(task.g, gpuByMem);
}

void getUsedAtTime(const vector<RunningJob>& jobs, int time, int& gpu, int& cpu, int& mem) {
    gpu = cpu = mem = 0;
    for (const RunningJob& job : jobs) {
        if (job.start <= time && time < job.finish) {
            gpu += job.gpu;
            cpu += job.cpu;
            mem += job.mem;
        }
    }
}

bool canPlaceAtTime(
    const Task& task,
    const Server& server,
    const vector<RunningJob>& jobs,
    int startTime,
    int useGpu
) {
    int finishTime = startTime + task.p;

    struct Event {
        int time;
        int gpu;
        int cpu;
        int mem;
    };

    vector<Event> events;
    events.reserve(jobs.size() * 2 + 2);
    events.push_back({startTime, 0, 0, 0});
    events.push_back({finishTime, 0, 0, 0});

    for (const RunningJob& job : jobs) {
        if (job.start < finishTime && job.finish > startTime) {
            int left = max(startTime, job.start);
            int right = min(finishTime, job.finish);
            events.push_back({left, job.gpu, job.cpu, job.mem});
            events.push_back({right, -job.gpu, -job.cpu, -job.mem});
        }
    }

    sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
        return a.time < b.time;
    });

    int usedGpu = 0;
    int usedCpu = 0;
    int usedMem = 0;
    int idx = 0;

    while (idx < (int)events.size()) {
        int now = events[idx].time;
        while (idx < (int)events.size() && events[idx].time == now) {
            usedGpu += events[idx].gpu;
            usedCpu += events[idx].cpu;
            usedMem += events[idx].mem;
            idx++;
        }

        int nextTime = (idx < (int)events.size() ? events[idx].time : finishTime);
        if (now < finishTime && nextTime > now) {
            if (usedGpu + useGpu > server.gpu) return false;
            if (usedCpu + task.c > server.cpu) return false;
            if (usedMem + task.m > server.mem) return false;
        }
    }

    return true;
}

vector<int> getCandidateTimes(const Task& task, const vector<RunningJob>& jobs, bool fullMode) {
    vector<int> cand;
    cand.reserve(jobs.size() + 1);
    cand.push_back(task.r);

    for (const RunningJob& job : jobs) {
        if (job.finish >= task.r) cand.push_back(job.finish);
    }

    sort(cand.begin(), cand.end());
    cand.erase(unique(cand.begin(), cand.end()), cand.end());

    if (fullMode) return cand;

    int cap;
    if (N > 4500) cap = 60;
    else if (N > 3500) cap = 30;
    else if (N > 2500) cap = 46;
    else if (N > 1500) cap = 80;
    else if (N > 800) cap = 120;
    else if (N > 350) cap = 180;
    else cap = 1000000000;

    if ((int)cand.size() <= cap) return cand;

    vector<int> reduced;
    int frontKeep = cap * 3 / 4;
    int backKeep = cap - frontKeep;

    for (int i = 0; i < frontKeep && i < (int)cand.size(); i++) {
        reduced.push_back(cand[i]);
    }
    for (int i = max(frontKeep, (int)cand.size() - backKeep); i < (int)cand.size(); i++) {
        reduced.push_back(cand[i]);
    }

    sort(reduced.begin(), reduced.end());
    reduced.erase(unique(reduced.begin(), reduced.end()), reduced.end());
    return reduced;
}

long long intervalWaste(
    const Task& task,
    const Server& server,
    const vector<RunningJob>& jobs,
    int startTime,
    int useGpu
) {
    int finishTime = startTime + task.p;
    vector<int> points;
    points.push_back(startTime);
    points.push_back(finishTime);

    for (const RunningJob& job : jobs) {
        if (job.start > startTime && job.start < finishTime) points.push_back(job.start);
        if (job.finish > startTime && job.finish < finishTime) points.push_back(job.finish);
    }

    sort(points.begin(), points.end());
    points.erase(unique(points.begin(), points.end()), points.end());

    long long total = 0;
    long long spanTotal = 0;

    for (int i = 0; i + 1 < (int)points.size(); i++) {
        int now = points[i];
        int next = points[i + 1];
        if (next <= now) continue;

        int usedGpu, usedCpu, usedMem;
        getUsedAtTime(jobs, now, usedGpu, usedCpu, usedMem);

        int leftGpu = server.gpu - usedGpu - useGpu;
        int leftCpu = server.cpu - usedCpu - task.c;
        int leftMem = server.mem - usedMem - task.m;
        long long waste = 1000LL * leftGpu + 10LL * leftCpu + leftMem;
        long long span = next - now;

        total += waste * span;
        spanTotal += span;
    }

    return spanTotal > 0 ? total / spanTotal : 0;
}

Metrics evaluatePlan(const vector<Answer>& ans) {
    Metrics metrics;

    long long totalGpuMem = 0;
    for (int s = 1; s <= M; s++) {
        totalGpuMem += 1LL * servers[s].gpu * servers[s].gpuMem;
    }

    vector<pair<int, long long>> events;
    events.reserve(N * 2 + 2);

    for (int i = 1; i <= N; i++) {
        const Answer& a = ans[i];
        const Task& task = tasks[i];
        metrics.wait += 1LL * task.w * (a.start - task.r);
        metrics.finish = max(metrics.finish, a.finish);
        events.push_back({a.start, task.v});
        events.push_back({a.finish, -1LL * task.v});
    }

    sort(events.begin(), events.end());

    long double idleArea = 0;
    long long usedMem = 0;
    int prevTime = 0;
    int idx = 0;

    while (idx < (int)events.size()) {
        int now = events[idx].first;
        if (now > prevTime) {
            idleArea += (long double)(totalGpuMem - usedMem) * (now - prevTime);
            prevTime = now;
        }

        while (idx < (int)events.size() && events[idx].first == now) {
            usedMem += events[idx].second;
            idx++;
        }
    }

    if (metrics.finish > prevTime) {
        idleArea += (long double)(totalGpuMem - usedMem) * (metrics.finish - prevTime);
    }

    metrics.idleMem = metrics.finish > 0 ? idleArea / metrics.finish : 0;
    return metrics;
}

long long placementScore(
    const Task& task,
    const Server& server,
    const vector<RunningJob>& jobs,
    int startTime,
    int useGpu,
    const Weight& weight
) {
    int finishTime = startTime + task.p;

    int usedGpu, usedCpu, usedMem;
    getUsedAtTime(jobs, startTime, usedGpu, usedCpu, usedMem);

    int leftGpu = server.gpu - usedGpu - useGpu;
    int leftCpu = server.cpu - usedCpu - task.c;
    int leftMem = server.mem - usedMem - task.m;

    long long waitCost = 1LL * task.w * (startTime - task.r);
    long long finishCost = finishTime;
    long long gpuWaste = 1LL * useGpu * server.gpuMem - task.v;
    long long resourceWaste = 1000LL * leftGpu + 10LL * leftCpu + leftMem;

    if (weight.mode == 1) {
        resourceWaste = intervalWaste(task, server, jobs, startTime, useGpu);
    }

    return waitCost * weight.waitW
         + finishCost * weight.finishW
         + gpuWaste * weight.gpuWasteW
         + resourceWaste * weight.resourceW;
}

Plan buildPlan(const vector<int>& order, const Weight& weight) {
    vector<vector<RunningJob>> serverJobs(M + 1);
    vector<Answer> ans(N + 1);

    for (int taskId : order) {
        const Task& task = tasks[taskId];

        int bestServer = -1;
        int bestStart = INT_MAX;
        int bestUseGpu = 0;
        long long bestScore = LLONG_MAX;

        auto tryAll = [&](bool fullMode) {
            for (int s = 1; s <= M; s++) {
                if (!feasible[taskId][s]) continue;

                int useGpu = minGpu[taskId][s];
                vector<int> cand = getCandidateTimes(task, serverJobs[s], fullMode);

                for (int startTime : cand) {
                    if (!canPlaceAtTime(task, servers[s], serverJobs[s], startTime, useGpu)) continue;

                    int finishTime = startTime + task.p;
                    long long score = placementScore(task, servers[s], serverJobs[s], startTime, useGpu, weight);

                    bool better = false;
                    if (bestServer == -1) {
                        better = true;
                    } else if (weight.mode == 2) {
                        if (startTime != bestStart) better = startTime < bestStart;
                        else if (score != bestScore) better = score < bestScore;
                        else if (finishTime != bestStart + task.p) better = finishTime < bestStart + task.p;
                        else better = s < bestServer;
                    } else if (weight.mode == 3) {
                        if (finishTime != bestStart + task.p) better = finishTime < bestStart + task.p;
                        else if (score != bestScore) better = score < bestScore;
                        else if (startTime != bestStart) better = startTime < bestStart;
                        else better = s < bestServer;
                    } else {
                        better =
                            score < bestScore ||
                            (score == bestScore && finishTime < bestStart + task.p) ||
                            (score == bestScore && finishTime == bestStart + task.p && s < bestServer);
                    }

                    if (better) {
                        bestScore = score;
                        bestServer = s;
                        bestStart = startTime;
                        bestUseGpu = useGpu;
                    }
                }
            }
        };

        tryAll(false);
        if (bestServer == -1) {
            tryAll(true);
        }

        if (bestServer == -1) {
            // The task statement normally guarantees feasibility. This fallback avoids crashing on bad data.
            for (int s = 1; s <= M && bestServer == -1; s++) {
                if (!feasible[taskId][s]) continue;
                int latest = task.r;
                for (const RunningJob& job : serverJobs[s]) latest = max(latest, job.finish);
                bestServer = s;
                bestStart = latest;
                bestUseGpu = minGpu[taskId][s];
            }
        }

        int finishTime = bestStart + task.p;
        ans[taskId] = {taskId, bestServer, bestStart, bestUseGpu, finishTime};

        serverJobs[bestServer].push_back({
            taskId,
            bestStart,
            finishTime,
            bestUseGpu,
            task.c,
            task.m,
            task.v
        });
    }

    Plan plan;
    plan.ans = move(ans);
    plan.metrics = evaluatePlan(plan.ans);
    return plan;
}

long double localObjective(
    const Metrics& metrics,
    long double waitW,
    long double idleW,
    long double finishW
) {
    return (long double)metrics.wait * waitW
         + metrics.idleMem * idleW
         + (long double)metrics.finish * finishW;
}

vector<vector<RunningJob>> buildJobsFromAnswer(const vector<Answer>& ans) {
    vector<vector<RunningJob>> serverJobs(M + 1);

    for (int i = 1; i <= N; i++) {
        const Answer& a = ans[i];
        const Task& task = tasks[i];
        serverJobs[a.serverId].push_back({
            i,
            a.start,
            a.finish,
            a.useGpu,
            task.c,
            task.m,
            task.v
        });
    }

    return serverJobs;
}

void removeTaskFromJobs(vector<RunningJob>& jobs, int taskId) {
    for (int i = 0; i < (int)jobs.size(); i++) {
        if (jobs[i].taskId == taskId) {
            jobs.erase(jobs.begin() + i);
            return;
        }
    }
}

Plan improveByReinsert(
    const Plan& basePlan,
    long double waitW,
    long double idleW,
    long double finishW,
    chrono::steady_clock::time_point startClock,
    double timeLimit
) {
    auto elapsed = [&]() {
        return chrono::duration<double>(chrono::steady_clock::now() - startClock).count();
    };

    vector<Answer> ans = basePlan.ans;
    Metrics currentMetrics = basePlan.metrics;
    long double currentValue = localObjective(currentMetrics, waitW, idleW, finishW);
    vector<vector<RunningJob>> serverJobs = buildJobsFromAnswer(ans);

    vector<int> order;
    order.reserve(N);
    for (int i = 1; i <= N; i++) order.push_back(i);

    sort(order.begin(), order.end(), [&](int a, int b) {
        long long waitA = 1LL * tasks[a].w * (ans[a].start - tasks[a].r);
        long long waitB = 1LL * tasks[b].w * (ans[b].start - tasks[b].r);
        if (waitA != waitB) return waitA > waitB;
        if (ans[a].finish != ans[b].finish) return ans[a].finish > ans[b].finish;
        if (tasks[a].w != tasks[b].w) return tasks[a].w > tasks[b].w;
        return resourceNeed(a) > resourceNeed(b);
    });

    int limit = min(N, N <= 80 ? 80 : 50);
    for (int pos = 0; pos < limit; pos++) {
        if (elapsed() > timeLimit) break;

        int taskId = order[pos];
        const Task& task = tasks[taskId];
        Answer oldAnswer = ans[taskId];

        removeTaskFromJobs(serverJobs[oldAnswer.serverId], taskId);

        Answer bestAnswer = oldAnswer;
        Metrics bestMetrics = currentMetrics;
        long double bestValue = currentValue;

        for (int s = 1; s <= M; s++) {
            if (!feasible[taskId][s]) continue;

            int useGpu = minGpu[taskId][s];
            vector<int> cand = getCandidateTimes(task, serverJobs[s], true);

            for (int startTime : cand) {
                if (elapsed() > timeLimit) break;
                if (!canPlaceAtTime(task, servers[s], serverJobs[s], startTime, useGpu)) continue;

                int finishTime = startTime + task.p;
                if (s == oldAnswer.serverId &&
                    startTime == oldAnswer.start &&
                    useGpu == oldAnswer.useGpu &&
                    finishTime == oldAnswer.finish) {
                    continue;
                }

                vector<Answer> trialAns = ans;
                trialAns[taskId] = {taskId, s, startTime, useGpu, finishTime};
                Metrics trialMetrics = evaluatePlan(trialAns);
                long double trialValue = localObjective(trialMetrics, waitW, idleW, finishW);

                if (trialValue + 1e-9L < bestValue ||
                    (fabsl(trialValue - bestValue) < 1e-9L && finishTime < bestAnswer.finish)) {
                    bestValue = trialValue;
                    bestMetrics = trialMetrics;
                    bestAnswer = trialAns[taskId];
                }
            }
        }

        ans[taskId] = bestAnswer;
        serverJobs[bestAnswer.serverId].push_back({
            taskId,
            bestAnswer.start,
            bestAnswer.finish,
            bestAnswer.useGpu,
            task.c,
            task.m,
            task.v
        });

        if (bestAnswer.serverId != oldAnswer.serverId ||
            bestAnswer.start != oldAnswer.start ||
            bestAnswer.useGpu != oldAnswer.useGpu) {
            currentMetrics = bestMetrics;
            currentValue = bestValue;
        }
    }

    Plan improved;
    improved.ans = move(ans);
    improved.metrics = evaluatePlan(improved.ans);
    return improved;
}

vector<vector<int>> makeOrders() {
    vector<vector<int>> orders;
    vector<int> base(N);
    iota(base.begin(), base.end(), 1);

    vector<int> feasibleCnt(N + 1, 0);
    for (int i = 1; i <= N; i++) {
        for (int s = 1; s <= M; s++) feasibleCnt[i] += feasible[i][s];
    }

    auto addOrder = [&](auto cmp) {
        vector<int> ord = base;
        stable_sort(ord.begin(), ord.end(), cmp);
        orders.push_back(move(ord));
    };

    addOrder([&](int a, int b) {
        if (tasks[a].r != tasks[b].r) return tasks[a].r < tasks[b].r;
        if (tasks[a].w != tasks[b].w) return tasks[a].w > tasks[b].w;
        long long lhs = 1LL * tasks[a].w * max(1, tasks[b].p);
        long long rhs = 1LL * tasks[b].w * max(1, tasks[a].p);
        if (lhs != rhs) return lhs > rhs;
        return resourceNeed(a) > resourceNeed(b);
    });

    addOrder([&](int a, int b) {
        if (tasks[a].r != tasks[b].r) return tasks[a].r < tasks[b].r;
        if (tasks[a].w != tasks[b].w) return tasks[a].w > tasks[b].w;
        if (tasks[a].p != tasks[b].p) return tasks[a].p < tasks[b].p;
        return resourceNeed(a) > resourceNeed(b);
    });

    addOrder([&](int a, int b) {
        if (tasks[a].w != tasks[b].w) return tasks[a].w > tasks[b].w;
        if (tasks[a].r != tasks[b].r) return tasks[a].r < tasks[b].r;
        return resourceNeed(a) > resourceNeed(b);
    });

    addOrder([&](int a, int b) {
        if (feasibleCnt[a] != feasibleCnt[b]) return feasibleCnt[a] < feasibleCnt[b];
        if (tasks[a].r != tasks[b].r) return tasks[a].r < tasks[b].r;
        if (tasks[a].w != tasks[b].w) return tasks[a].w > tasks[b].w;
        return resourceNeed(a) > resourceNeed(b);
    });

    addOrder([&](int a, int b) {
        long long pressureA = resourceNeed(a) / max(1, feasibleCnt[a]);
        long long pressureB = resourceNeed(b) / max(1, feasibleCnt[b]);
        if (pressureA != pressureB) return pressureA > pressureB;
        if (tasks[a].r != tasks[b].r) return tasks[a].r < tasks[b].r;
        return tasks[a].w > tasks[b].w;
    });

    addOrder([&](int a, int b) {
        if (resourceNeed(a) != resourceNeed(b)) return resourceNeed(a) > resourceNeed(b);
        if (tasks[a].r != tasks[b].r) return tasks[a].r < tasks[b].r;
        return tasks[a].w > tasks[b].w;
    });

    addOrder([&](int a, int b) {
        long long lhs = 1LL * tasks[a].w * max(1, tasks[b].p);
        long long rhs = 1LL * tasks[b].w * max(1, tasks[a].p);
        if (lhs != rhs) return lhs > rhs;
        if (tasks[a].r != tasks[b].r) return tasks[a].r < tasks[b].r;
        return resourceNeed(a) > resourceNeed(b);
    });

    addOrder([&](int a, int b) {
        long long scoreA = 1LL * tasks[a].w * max(1, tasks[a].p);
        long long scoreB = 1LL * tasks[b].w * max(1, tasks[b].p);
        if (scoreA != scoreB) return scoreA > scoreB;
        if (tasks[a].r != tasks[b].r) return tasks[a].r < tasks[b].r;
        return resourceNeed(a) > resourceNeed(b);
    });

    addOrder([&](int a, int b) {
        int bucketA = tasks[a].r / 250;
        int bucketB = tasks[b].r / 250;
        if (bucketA != bucketB) return bucketA < bucketB;
        long long pressureA = resourceNeed(a) / max(1, feasibleCnt[a]);
        long long pressureB = resourceNeed(b) / max(1, feasibleCnt[b]);
        if (pressureA != pressureB) return pressureA > pressureB;
        return tasks[a].w > tasks[b].w;
    });

    addOrder([&](int a, int b) {
        int bucketA = tasks[a].r / 80;
        int bucketB = tasks[b].r / 80;
        if (bucketA != bucketB) return bucketA < bucketB;
        long long lhs = 1LL * tasks[a].w * max(1, tasks[b].p);
        long long rhs = 1LL * tasks[b].w * max(1, tasks[a].p);
        if (lhs != rhs) return lhs > rhs;
        return resourceNeed(a) > resourceNeed(b);
    });

    addOrder([&](int a, int b) {
        int bucketA = tasks[a].r / 500;
        int bucketB = tasks[b].r / 500;
        if (bucketA != bucketB) return bucketA < bucketB;
        if (tasks[a].w != tasks[b].w) return tasks[a].w > tasks[b].w;
        if (tasks[a].p != tasks[b].p) return tasks[a].p < tasks[b].p;
        return resourceNeed(a) > resourceNeed(b);
    });

    sort(orders.begin(), orders.end());
    orders.erase(unique(orders.begin(), orders.end()), orders.end());
    return orders;
}

double normalizedScore(const Metrics& m, const Metrics& mn, const Metrics& mx) {
    auto norm = [](long double x, long double lo, long double hi) {
        if (fabsl(hi - lo) < 1e-12L) return 0.0L;
        return (x - lo) / (hi - lo);
    };

    long double waitN = norm(m.wait, mn.wait, mx.wait);
    long double idleN = norm(m.idleMem, mn.idleMem, mx.idleMem);
    long double finishN = norm(m.finish, mn.finish, mx.finish);

    return (double)(waitN + idleN + finishN);
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    cin >> M >> N;
    servers.assign(M + 1, Server());
    tasks.assign(N + 1, Task());

    for (int i = 1; i <= M; i++) {
        servers[i].id = i;
        cin >> servers[i].gpu >> servers[i].gpuMem >> servers[i].cpu >> servers[i].mem;
    }

    for (int i = 1; i <= N; i++) {
        tasks[i].id = i;
        cin >> tasks[i].r >> tasks[i].p >> tasks[i].g >> tasks[i].v
            >> tasks[i].c >> tasks[i].m >> tasks[i].w;
    }

    minGpu.assign(N + 1, vector<int>(M + 1, 0));
    feasible.assign(N + 1, vector<char>(M + 1, 0));

    for (int i = 1; i <= N; i++) {
        for (int s = 1; s <= M; s++) {
            int need = calcMinGpu(tasks[i], servers[s]);
            minGpu[i][s] = need;
            feasible[i][s] =
                need <= servers[s].gpu &&
                tasks[i].c <= servers[s].cpu &&
                tasks[i].m <= servers[s].mem;
        }
    }

    auto startClock = chrono::steady_clock::now();
    auto elapsed = [&]() {
        return chrono::duration<double>(chrono::steady_clock::now() - startClock).count();
    };

    vector<vector<int>> orders = makeOrders();
    vector<Weight> weights = {
        {1000000, 10000, 100, 1, 0},
        {1500000, 8000, 80, 1, 0},
        {2200000, 5000, 50, 1, 0},
        {3500000, 2000, 30, 1, 0},
        {5000000, 1000, 20, 0, 2},
        {2500000, 4000, 40, 1, 2},
        {1200000, 20000, 80, 1, 3},
        {800000, 25000, 120, 1, 0},
        {600000, 45000, 150, 2, 0},
        {1000000, 8000, 500, 3, 0},
        {500000, 12000, 1200, 8, 0},
        {1200000, 15000, 60, 0, 0},
        {900000, 12000, 300, 2, 1},
        {1500000, 8000, 80, 1, 1},
    };

    if (N > 4500) {
        vector<int> keepOrders = {0, 1, 2, 3, 9};
        vector<vector<int>> reduced;
        for (int idx : keepOrders) if (idx < (int)orders.size()) reduced.push_back(orders[idx]);
        if (!reduced.empty()) orders = move(reduced);
        weights = {
            {1000000, 10000, 100, 1, 0},
            {2200000, 5000, 50, 1, 0},
            {3500000, 2000, 30, 1, 0},
            {5000000, 1000, 20, 0, 2},
            {1200000, 20000, 80, 1, 3},
        };
    } else if (N > 3000) {
        vector<int> keepOrders = {0, 1, 2, 3, 4, 9};
        vector<vector<int>> reduced;
        for (int idx : keepOrders) if (idx < (int)orders.size()) reduced.push_back(orders[idx]);
        if (!reduced.empty()) orders = move(reduced);
        weights = {
            {1000000, 10000, 100, 1, 0},
            {1500000, 8000, 80, 1, 0},
            {2200000, 5000, 50, 1, 0},
            {3500000, 2000, 30, 1, 0},
            {5000000, 1000, 20, 0, 2},
            {800000, 25000, 120, 1, 0},
        };
    } else if (N > 1800) {
        vector<int> keepOrders = {0, 1, 2, 3, 4, 5, 9, 10};
        vector<vector<int>> reduced;
        for (int idx : keepOrders) if (idx < (int)orders.size()) reduced.push_back(orders[idx]);
        if (!reduced.empty()) orders = move(reduced);
        weights = {
            {1000000, 10000, 100, 1, 0},
            {1500000, 8000, 80, 1, 0},
            {2200000, 5000, 50, 1, 0},
            {2500000, 4000, 40, 1, 2},
            {800000, 25000, 120, 1, 0},
            {600000, 45000, 150, 2, 0},
            {1000000, 8000, 500, 3, 0},
        };
    }

    vector<Plan> plans;
    bool useLocalSearch = (N <= 220);
    bool stop = false;

    for (int i = 0; i < (int)orders.size() && !stop; i++) {
        for (int j = 0; j < (int)weights.size(); j++) {
            if (elapsed() > 54.0) {
                stop = true;
                break;
            }
            Plan basePlan = buildPlan(orders[i], weights[j]);
            plans.push_back(basePlan);

            if (useLocalSearch && elapsed() < 50.0) {
                plans.push_back(improveByReinsert(
                    basePlan,
                    1.0L,
                    260.0L,
                    55.0L,
                    startClock,
                    53.0
                ));
            }
        }
    }

    if (plans.empty()) {
        plans.push_back(buildPlan(orders[0], weights[0]));
    }

    Metrics mn = plans[0].metrics;
    Metrics mx = plans[0].metrics;
    for (int i = 1; i < (int)plans.size(); i++) {
        mn.wait = min(mn.wait, plans[i].metrics.wait);
        mn.idleMem = min(mn.idleMem, plans[i].metrics.idleMem);
        mn.finish = min(mn.finish, plans[i].metrics.finish);

        mx.wait = max(mx.wait, plans[i].metrics.wait);
        mx.idleMem = max(mx.idleMem, plans[i].metrics.idleMem);
        mx.finish = max(mx.finish, plans[i].metrics.finish);
    }

    int best = 0;
    double bestValue = normalizedScore(plans[0].metrics, mn, mx);
    for (int i = 1; i < (int)plans.size(); i++) {
        double value = normalizedScore(plans[i].metrics, mn, mx);
        if (value < bestValue) {
            bestValue = value;
            best = i;
        }
    }

    const vector<Answer>& ans = plans[best].ans;
    for (int i = 1; i <= N; i++) {
        cout << ans[i].taskId << ' '
             << ans[i].serverId << ' '
             << ans[i].start << ' '
             << ans[i].useGpu << ' '
             << ans[i].finish << '\n';
    }

    return 0;
}
