#include <bits/stdc++.h>
using namespace std;

struct Server {
    int id;
    int gpu;
    int gpuMem;
    int cpu;
    int mem;
};

struct Task {
    int id;
    int submitTime;
    int runTime;
    int needGpu;
    int needGpuMem;
    int needCpu;
    int needMem;
    int priority;
};

struct RunningJob {
    int taskId;
    int startTime;
    int finishTime;
    int useGpu;
    int useCpu;
    int useMem;
    int useGpuMem;
};

struct Answer {
    int taskId = 0;
    int serverId = -1;
    int startTime = 0;
    int useGpu = 0;
    int finishTime = 0;
};

struct Metrics {
    long long wait = 0;
    double idleMem = 0;
    int finish = 0;
};

struct Plan {
    vector<Answer> ans;
    Metrics metrics;
};

struct ScoreWeight {
    long long waitW;
    long long finishW;
    long long gpuWasteW;
    long long resourceW;
    int mode;
};

long double localObjective(const Metrics& m, long double waitW, long double idleW, long double finishW) {
    return (long double)m.wait * waitW + (long double)m.idleMem * idleW + (long double)m.finish * finishW;
}

int M, N;
vector<Server> servers;
vector<Task> tasks;

long long resourceNeed(int i) {
    return 1LL * tasks[i].needGpu * 1000000
         + 1LL * tasks[i].needGpuMem * 1000
         + 1LL * tasks[i].needCpu * 20
         + tasks[i].needMem;
}

int calcNeedGpu(const Task& task, const Server& server) {
    int gpuByMem = (task.needGpuMem + server.gpuMem - 1) / server.gpuMem;
    return max(task.needGpu, gpuByMem);
}

bool canRunOnServer(const Task& task, const Server& server) {
    int needGpu = calcNeedGpu(task, server);
    return needGpu <= server.gpu && task.needCpu <= server.cpu && task.needMem <= server.mem;
}

void getUsedResourceAtTime(
    const vector<RunningJob>& jobs,
    int time,
    int& usedGpu,
    int& usedCpu,
    int& usedMem
) {
    usedGpu = 0;
    usedCpu = 0;
    usedMem = 0;

    for (int i = 0; i < (int)jobs.size(); i++) {
        if (jobs[i].startTime <= time && time < jobs[i].finishTime) {
            usedGpu += jobs[i].useGpu;
            usedCpu += jobs[i].useCpu;
            usedMem += jobs[i].useMem;
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
    int finishTime = startTime + task.runTime;
    vector<int> checkTimes;
    checkTimes.push_back(startTime);

    for (int i = 0; i < (int)jobs.size(); i++) {
        if (jobs[i].startTime >= startTime && jobs[i].startTime < finishTime) {
            checkTimes.push_back(jobs[i].startTime);
        }
        if (jobs[i].finishTime > startTime && jobs[i].finishTime < finishTime) {
            checkTimes.push_back(jobs[i].finishTime);
        }
    }

    for (int k = 0; k < (int)checkTimes.size(); k++) {
        int usedGpu, usedCpu, usedMem;
        getUsedResourceAtTime(jobs, checkTimes[k], usedGpu, usedCpu, usedMem);
        if (usedGpu + useGpu > server.gpu) return false;
        if (usedCpu + task.needCpu > server.cpu) return false;
        if (usedMem + task.needMem > server.mem) return false;
    }

    return true;
}

long long calcIntervalResourceWaste(
    const Task& task,
    const Server& server,
    const vector<RunningJob>& jobs,
    int startTime,
    int useGpu
) {
    int finishTime = startTime + task.runTime;
    vector<int> checkTimes;
    checkTimes.push_back(startTime);
    checkTimes.push_back(finishTime);

    for (int i = 0; i < (int)jobs.size(); i++) {
        if (jobs[i].startTime > startTime && jobs[i].startTime < finishTime) {
            checkTimes.push_back(jobs[i].startTime);
        }
        if (jobs[i].finishTime > startTime && jobs[i].finishTime < finishTime) {
            checkTimes.push_back(jobs[i].finishTime);
        }
    }

    sort(checkTimes.begin(), checkTimes.end());
    checkTimes.erase(unique(checkTimes.begin(), checkTimes.end()), checkTimes.end());

    long long totalWaste = 0;
    long long totalSpan = 0;

    for (int i = 0; i + 1 < (int)checkTimes.size(); i++) {
        int now = checkTimes[i];
        int nextTime = checkTimes[i + 1];
        if (nextTime <= now) continue;

        int usedGpu, usedCpu, usedMem;
        getUsedResourceAtTime(jobs, now, usedGpu, usedCpu, usedMem);

        int leftGpu = server.gpu - usedGpu - useGpu;
        int leftCpu = server.cpu - usedCpu - task.needCpu;
        int leftMem = server.mem - usedMem - task.needMem;

        long long span = nextTime - now;
        long long waste = 1LL * leftGpu * 1000 + 1LL * leftCpu * 10 + leftMem;

        totalWaste += waste * span;
        totalSpan += span;
    }

    if (totalSpan == 0) return 0;
    return totalWaste / totalSpan;
}

vector<int> getCandidateStartTimes(const Task& task, const vector<RunningJob>& jobs) {
    vector<int> candidateTimes;
    candidateTimes.push_back(task.submitTime);

    for (int i = 0; i < (int)jobs.size(); i++) {
        if (jobs[i].finishTime >= task.submitTime) {
            candidateTimes.push_back(jobs[i].finishTime);
        }
    }

    sort(candidateTimes.begin(), candidateTimes.end());
    candidateTimes.erase(unique(candidateTimes.begin(), candidateTimes.end()), candidateTimes.end());
    return candidateTimes;
}

Metrics evaluatePlan(const vector<Answer>& ans) {
    Metrics m;
    vector<vector<RunningJob>> jobs(M + 1);

    for (int i = 1; i <= N; i++) {
        const Answer& a = ans[i];
        const Task& task = tasks[i];
        m.wait += 1LL * task.priority * (a.startTime - task.submitTime);
        m.finish = max(m.finish, a.finishTime);
        jobs[a.serverId].push_back({
            i,
            a.startTime,
            a.finishTime,
            a.useGpu,
            task.needCpu,
            task.needMem,
            task.needGpuMem
        });
    }

    long long totalGpuMem = 0;
    for (int s = 1; s <= M; s++) {
        totalGpuMem += 1LL * servers[s].gpu * servers[s].gpuMem;
    }

    vector<int> events;
    events.push_back(0);
    events.push_back(m.finish);
    for (int s = 1; s <= M; s++) {
        for (int k = 0; k < (int)jobs[s].size(); k++) {
            events.push_back(jobs[s][k].startTime);
            events.push_back(jobs[s][k].finishTime);
        }
    }
    sort(events.begin(), events.end());
    events.erase(unique(events.begin(), events.end()), events.end());

    long double idleArea = 0;
    for (int e = 0; e + 1 < (int)events.size(); e++) {
        int left = events[e];
        int right = events[e + 1];
        if (right <= left) continue;

        long long usedTaskMem = 0;
        for (int s = 1; s <= M; s++) {
            for (int k = 0; k < (int)jobs[s].size(); k++) {
                if (jobs[s][k].startTime <= left && left < jobs[s][k].finishTime) {
                    usedTaskMem += jobs[s][k].useGpuMem;
                }
            }
        }
        idleArea += (long double)(totalGpuMem - usedTaskMem) * (right - left);
    }

    m.idleMem = m.finish > 0 ? (double)(idleArea / m.finish) : 0.0;
    return m;
}

Plan buildPlan(vector<int> order, const ScoreWeight& w) {
    vector<vector<RunningJob>> serverJobs(M + 1);
    vector<Answer> ans(N + 1);

    for (int idx = 0; idx < (int)order.size(); idx++) {
        int i = order[idx];

        int bestServer = -1;
        int bestStartTime = INT_MAX;
        int bestUseGpu = 0;
        long long bestScore = (1LL << 62);

        for (int s = 1; s <= M; s++) {
            if (!canRunOnServer(tasks[i], servers[s])) continue;

            int useGpu = calcNeedGpu(tasks[i], servers[s]);
            vector<int> candidateTimes = getCandidateStartTimes(tasks[i], serverJobs[s]);

            for (int p = 0; p < (int)candidateTimes.size(); p++) {
                int startTime = candidateTimes[p];
                if (!canPlaceAtTime(tasks[i], servers[s], serverJobs[s], startTime, useGpu)) {
                    continue;
                }

                int finishTime = startTime + tasks[i].runTime;
                int usedGpuNow, usedCpuNow, usedMemNow;
                getUsedResourceAtTime(serverJobs[s], startTime, usedGpuNow, usedCpuNow, usedMemNow);

                int leftGpu = servers[s].gpu - usedGpuNow - useGpu;
                int leftCpu = servers[s].cpu - usedCpuNow - tasks[i].needCpu;
                int leftMem = servers[s].mem - usedMemNow - tasks[i].needMem;

                long long waitCost = 1LL * tasks[i].priority * (startTime - tasks[i].submitTime);
                long long gpuMemWaste = 1LL * useGpu * servers[s].gpuMem - tasks[i].needGpuMem;
                long long resourceWaste = 1LL * leftGpu * 1000 + 1LL * leftCpu * 10 + leftMem;
                if (w.mode == 1) {
                    resourceWaste = calcIntervalResourceWaste(
                        tasks[i],
                        servers[s],
                        serverJobs[s],
                        startTime,
                        useGpu
                    );
                }

                long long score =
                    waitCost * w.waitW +
                    1LL * finishTime * w.finishW +
                    gpuMemWaste * w.gpuWasteW +
                    resourceWaste * w.resourceW;

                if (score < bestScore ||
                    (score == bestScore && finishTime < bestStartTime + tasks[i].runTime)) {
                    bestScore = score;
                    bestServer = s;
                    bestStartTime = startTime;
                    bestUseGpu = useGpu;
                }
            }
        }

        int finishTime = bestStartTime + tasks[i].runTime;
        ans[i] = {tasks[i].id, bestServer, bestStartTime, bestUseGpu, finishTime};

        serverJobs[bestServer].push_back({
            i,
            bestStartTime,
            finishTime,
            bestUseGpu,
            tasks[i].needCpu,
            tasks[i].needMem,
            tasks[i].needGpuMem
        });
    }

    Plan plan;
    plan.ans = ans;
    plan.metrics = evaluatePlan(ans);
    return plan;
}

vector<vector<RunningJob>> buildJobsFromAnswer(const vector<Answer>& ans) {
    vector<vector<RunningJob>> serverJobs(M + 1);

    for (int i = 1; i <= N; i++) {
        const Answer& a = ans[i];
        serverJobs[a.serverId].push_back({
            i,
            a.startTime,
            a.finishTime,
            a.useGpu,
            tasks[i].needCpu,
            tasks[i].needMem,
            tasks[i].needGpuMem
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

Plan improveByReinsert(const Plan& basePlan, long double waitW, long double idleW, long double finishW) {
    Plan bestPlan = basePlan;
    vector<Answer> ans = basePlan.ans;
    Metrics currentMetrics = basePlan.metrics;
    long double currentValue = localObjective(currentMetrics, waitW, idleW, finishW);

    int rounds = 1;
    for (int round = 0; round < rounds; round++) {
        bool changed = false;
        vector<vector<RunningJob>> serverJobs = buildJobsFromAnswer(ans);

        vector<int> order;
        for (int i = 1; i <= N; i++) {
            order.push_back(i);
        }

        sort(order.begin(), order.end(), [&](int a, int b) {
            long long waitA = 1LL * tasks[a].priority * (ans[a].startTime - tasks[a].submitTime);
            long long waitB = 1LL * tasks[b].priority * (ans[b].startTime - tasks[b].submitTime);
            if (waitA != waitB) return waitA > waitB;
            if (ans[a].finishTime != ans[b].finishTime) return ans[a].finishTime > ans[b].finishTime;
            return tasks[a].priority > tasks[b].priority;
        });

        int limit = min(N, 40);
        for (int pos = 0; pos < limit; pos++) {
            int taskId = order[pos];
            Answer oldAnswer = ans[taskId];
            removeTaskFromJobs(serverJobs[oldAnswer.serverId], taskId);

            Answer bestAnswer = oldAnswer;
            Metrics bestMetrics = currentMetrics;
            long double bestValue = currentValue;

            for (int s = 1; s <= M; s++) {
                if (!canRunOnServer(tasks[taskId], servers[s])) continue;

                int useGpu = calcNeedGpu(tasks[taskId], servers[s]);
                vector<int> candidateTimes = getCandidateStartTimes(tasks[taskId], serverJobs[s]);

                for (int p = 0; p < (int)candidateTimes.size(); p++) {
                    int startTime = candidateTimes[p];
                    if (!canPlaceAtTime(tasks[taskId], servers[s], serverJobs[s], startTime, useGpu)) {
                        continue;
                    }

                    int finishTime = startTime + tasks[taskId].runTime;
                    if (startTime == oldAnswer.startTime &&
                        finishTime == oldAnswer.finishTime &&
                        s == oldAnswer.serverId &&
                        useGpu == oldAnswer.useGpu) {
                        continue;
                    }

                    vector<Answer> trialAns = ans;
                    trialAns[taskId] = {taskId, s, startTime, useGpu, finishTime};
                    Metrics trialMetrics = evaluatePlan(trialAns);
                    long double trialValue = localObjective(trialMetrics, waitW, idleW, finishW);

                    if (trialValue + 1e-9L < bestValue) {
                        bestValue = trialValue;
                        bestMetrics = trialMetrics;
                        bestAnswer = trialAns[taskId];
                    }
                }
            }

            ans[taskId] = bestAnswer;
            serverJobs[bestAnswer.serverId].push_back({
                taskId,
                bestAnswer.startTime,
                bestAnswer.finishTime,
                bestAnswer.useGpu,
                tasks[taskId].needCpu,
                tasks[taskId].needMem,
                tasks[taskId].needGpuMem
            });

            if (bestAnswer.serverId != oldAnswer.serverId ||
                bestAnswer.startTime != oldAnswer.startTime ||
                bestAnswer.useGpu != oldAnswer.useGpu) {
                currentMetrics = bestMetrics;
                currentValue = bestValue;
                changed = true;
            }
        }

        if (!changed) break;
    }

    bestPlan.ans = ans;
    bestPlan.metrics = evaluatePlan(ans);
    return bestPlan;
}

vector<vector<int>> makeOrders() {
    vector<vector<int>> orders;
    vector<int> base;
    for (int i = 1; i <= N; i++) base.push_back(i);

    auto addOrder = [&](auto cmp) {
        vector<int> ord = base;
        stable_sort(ord.begin(), ord.end(), cmp);
        orders.push_back(ord);
    };

    vector<int> feasibleCnt(N + 1, 0);
    for (int i = 1; i <= N; i++) {
        for (int s = 1; s <= M; s++) {
            if (canRunOnServer(tasks[i], servers[s])) feasibleCnt[i]++;
        }
    }

    addOrder([&](int a, int b) {
        if (tasks[a].submitTime != tasks[b].submitTime) return tasks[a].submitTime < tasks[b].submitTime;
        if (tasks[a].priority != tasks[b].priority) return tasks[a].priority > tasks[b].priority;
        if (tasks[a].runTime != tasks[b].runTime) return tasks[a].runTime < tasks[b].runTime;
        return resourceNeed(a) > resourceNeed(b);
    });

    addOrder([&](int a, int b) {
        if (tasks[a].submitTime != tasks[b].submitTime) return tasks[a].submitTime < tasks[b].submitTime;
        if (tasks[a].priority != tasks[b].priority) return tasks[a].priority > tasks[b].priority;
        return resourceNeed(a) > resourceNeed(b);
    });

    addOrder([&](int a, int b) {
        if (tasks[a].submitTime != tasks[b].submitTime) return tasks[a].submitTime < tasks[b].submitTime;
        long long lhs = 1LL * tasks[a].priority * tasks[b].runTime;
        long long rhs = 1LL * tasks[b].priority * tasks[a].runTime;
        if (lhs != rhs) return lhs > rhs;
        return resourceNeed(a) > resourceNeed(b);
    });

    addOrder([&](int a, int b) {
        if (feasibleCnt[a] != feasibleCnt[b]) return feasibleCnt[a] < feasibleCnt[b];
        if (tasks[a].submitTime != tasks[b].submitTime) return tasks[a].submitTime < tasks[b].submitTime;
        if (tasks[a].priority != tasks[b].priority) return tasks[a].priority > tasks[b].priority;
        return resourceNeed(a) > resourceNeed(b);
    });

    addOrder([&](int a, int b) {
        if (tasks[a].priority != tasks[b].priority) return tasks[a].priority > tasks[b].priority;
        if (tasks[a].submitTime != tasks[b].submitTime) return tasks[a].submitTime < tasks[b].submitTime;
        return resourceNeed(a) > resourceNeed(b);
    });

    addOrder([&](int a, int b) {
        if (resourceNeed(a) != resourceNeed(b)) return resourceNeed(a) > resourceNeed(b);
        if (tasks[a].submitTime != tasks[b].submitTime) return tasks[a].submitTime < tasks[b].submitTime;
        return tasks[a].priority > tasks[b].priority;
    });

    addOrder([&](int a, int b) {
        long long lhs = 1LL * tasks[a].priority * tasks[b].runTime;
        long long rhs = 1LL * tasks[b].priority * tasks[a].runTime;
        if (lhs != rhs) return lhs > rhs;
        if (tasks[a].submitTime != tasks[b].submitTime) return tasks[a].submitTime < tasks[b].submitTime;
        return resourceNeed(a) > resourceNeed(b);
    });

    addOrder([&](int a, int b) {
        if (tasks[a].submitTime != tasks[b].submitTime) return tasks[a].submitTime < tasks[b].submitTime;
        if (tasks[a].runTime != tasks[b].runTime) return tasks[a].runTime < tasks[b].runTime;
        if (tasks[a].priority != tasks[b].priority) return tasks[a].priority > tasks[b].priority;
        return resourceNeed(a) > resourceNeed(b);
    });

    addOrder([&](int a, int b) {
        if (feasibleCnt[a] != feasibleCnt[b]) return feasibleCnt[a] < feasibleCnt[b];
        if (resourceNeed(a) != resourceNeed(b)) return resourceNeed(a) > resourceNeed(b);
        if (tasks[a].priority != tasks[b].priority) return tasks[a].priority > tasks[b].priority;
        return tasks[a].submitTime < tasks[b].submitTime;
    });

    addOrder([&](int a, int b) {
        long long pressureA = resourceNeed(a) / max(1, feasibleCnt[a]);
        long long pressureB = resourceNeed(b) / max(1, feasibleCnt[b]);
        if (pressureA != pressureB) return pressureA > pressureB;
        if (tasks[a].submitTime != tasks[b].submitTime) return tasks[a].submitTime < tasks[b].submitTime;
        return tasks[a].priority > tasks[b].priority;
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

    return (double)(0.55L * waitN + 0.20L * idleN + 0.25L * finishN);
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
        cin >> tasks[i].submitTime
            >> tasks[i].runTime
            >> tasks[i].needGpu
            >> tasks[i].needGpuMem
            >> tasks[i].needCpu
            >> tasks[i].needMem
            >> tasks[i].priority;
    }

    auto startClock = chrono::steady_clock::now();
    auto elapsedSeconds = [&]() {
        chrono::duration<double> diff = chrono::steady_clock::now() - startClock;
        return diff.count();
    };

    vector<vector<int>> orders = makeOrders();
    vector<ScoreWeight> weights = {
        {1000000, 10000, 100, 1, 0},
        {1500000, 8000, 80, 1, 0},
        {600000, 30000, 100, 1, 0},
        {800000, 12000, 400, 3, 0},
        {400000, 20000, 800, 6, 0},
        {2000000, 5000, 50, 1, 0},
        {3000000, 2000, 50, 1, 0},
        {1000000, 50000, 100, 1, 0},
        {300000, 60000, 300, 3, 0},
        {500000, 10000, 1500, 10, 0},
        {1200000, 15000, 30, 0, 0},
        {700000, 25000, 600, 2, 0},
        {1000000, 10000, 100, 1, 1},
        {1500000, 8000, 80, 1, 1},
        {800000, 12000, 400, 3, 1},
        {500000, 10000, 1500, 10, 1},
        {700000, 25000, 600, 2, 1}
    };

    vector<Plan> plans;
    vector<array<long double, 3>> localWeights = {
        {1.0L, 300.0L, 50.0L}
    };

    bool useLocalSearch = (N <= 50);
    bool stop = false;
    for (int i = 0; i < (int)orders.size() && !stop; i++) {
        for (int j = 0; j < (int)weights.size(); j++) {
            if (elapsedSeconds() > 54.0) {
                stop = true;
                break;
            }
            Plan basePlan = buildPlan(orders[i], weights[j]);
            plans.push_back(basePlan);
            for (int k = 0; useLocalSearch && k < (int)localWeights.size(); k++) {
                if (elapsedSeconds() > 54.0) {
                    stop = true;
                    break;
                }
                plans.push_back(improveByReinsert(
                    basePlan,
                    localWeights[k][0],
                    localWeights[k][1],
                    localWeights[k][2]
                ));
            }
        }
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
        cout << ans[i].taskId << " "
             << ans[i].serverId << " "
             << ans[i].startTime << " "
             << ans[i].useGpu << " "
             << ans[i].finishTime << "\n";
    }

    return 0;
}
