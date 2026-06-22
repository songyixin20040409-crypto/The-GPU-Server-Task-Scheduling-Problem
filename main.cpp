#include <bits/stdc++.h>
using namespace std;

// 代码版本五：并发调度 + 任务排序 + 多候选开始时间 + 综合评分选方案
// 每台服务器尝试多个候选时间：
// submitTime、已有任务结束时间
// 然后从所有可行方案中选评分最低的
struct Server {
    int id;
    int gpu;
    int gpuMem; // 单张 GPU 显存
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
    int startTime;
    int finishTime;
    int useGpu;
    int useCpu;
    int useMem;
};

struct Answer {
    int taskId;
    int serverId;
    int startTime;
    int useGpu;
    int finishTime;
};

int calcNeedGpu(const Task& task, const Server& server) {
    int gpuByMem = (task.needGpuMem + server.gpuMem - 1) / server.gpuMem;
    return max(task.needGpu, gpuByMem);
}

bool canRunOnServer(const Task& task, const Server& server) {
    int needGpu = calcNeedGpu(task, server);

    if (needGpu > server.gpu) return false;
    if (task.needCpu > server.cpu) return false;
    if (task.needMem > server.mem) return false;

    return true;
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

    for (int i = 0; i < jobs.size(); i++) {
        if (jobs[i].startTime >= startTime && jobs[i].startTime < finishTime) {
            checkTimes.push_back(jobs[i].startTime);
        }

        if (jobs[i].finishTime > startTime && jobs[i].finishTime < finishTime) {
            checkTimes.push_back(jobs[i].finishTime);
        }
    }

    for (int k = 0; k < checkTimes.size(); k++) {
        int now = checkTimes[k];

        int usedGpu = 0;
        int usedCpu = 0;
        int usedMem = 0;

        for (int i = 0; i < jobs.size(); i++) {
            if (jobs[i].startTime <= now && now < jobs[i].finishTime) {
                usedGpu += jobs[i].useGpu;
                usedCpu += jobs[i].useCpu;
                usedMem += jobs[i].useMem;
            }
        }

        if (usedGpu + useGpu > server.gpu) return false;
        if (usedCpu + task.needCpu > server.cpu) return false;
        if (usedMem + task.needMem > server.mem) return false;
    }

    return true;
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

    for (int i = 0; i < jobs.size(); i++) {
        if (jobs[i].startTime <= time && time < jobs[i].finishTime) {
            usedGpu += jobs[i].useGpu;
            usedCpu += jobs[i].useCpu;
            usedMem += jobs[i].useMem;
        }
    }
}

// 获取候选开始时间：任务提交时间 + 已有任务结束时间
vector<int> getCandidateStartTimes(
    const Task& task,
    const vector<RunningJob>& jobs
) {
    vector<int> candidateTimes;

    candidateTimes.push_back(task.submitTime);

    for (int i = 0; i < jobs.size(); i++) {
        if (jobs[i].finishTime >= task.submitTime) {
            candidateTimes.push_back(jobs[i].finishTime);
        }
    }

    sort(candidateTimes.begin(), candidateTimes.end());
    candidateTimes.erase(unique(candidateTimes.begin(), candidateTimes.end()), candidateTimes.end());

    return candidateTimes;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int M, N;
    cin >> M >> N;

    vector<Server> servers(M + 1);
    for (int i = 1; i <= M; i++) {
        servers[i].id = i;
        cin >> servers[i].gpu
            >> servers[i].gpuMem
            >> servers[i].cpu
            >> servers[i].mem;
    }

    vector<Task> tasks(N + 1);
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

    vector<int> order;
    for (int i = 1; i <= N; i++) {
        order.push_back(i);
    }

    sort(order.begin(), order.end(), [&](int a, int b) {
        if (tasks[a].submitTime != tasks[b].submitTime) {
            return tasks[a].submitTime < tasks[b].submitTime;
        }

        if (tasks[a].priority != tasks[b].priority) {
            return tasks[a].priority > tasks[b].priority;
        }

        if (tasks[a].runTime != tasks[b].runTime) {
            return tasks[a].runTime < tasks[b].runTime;
        }

        long long resourceA =
            1LL * tasks[a].needGpu * 1000000 +
            1LL * tasks[a].needGpuMem * 1000 +
            1LL * tasks[a].needCpu * 10 +
            tasks[a].needMem;

        long long resourceB =
            1LL * tasks[b].needGpu * 1000000 +
            1LL * tasks[b].needGpuMem * 1000 +
            1LL * tasks[b].needCpu * 10 +
            tasks[b].needMem;

        return resourceA > resourceB;
    });

    vector<vector<RunningJob>> serverJobs(M + 1);
    vector<Answer> ans(N + 1);

    for (int idx = 0; idx < order.size(); idx++) {
        int i = order[idx];

        int bestServer = -1;
        int bestStartTime = 1e9;
        int bestUseGpu = 0;
        long long bestScore = (1LL << 62);

        for (int j = 1; j <= M; j++) {
            if (!canRunOnServer(tasks[i], servers[j])) {
                continue;
            }

            int useGpu = calcNeedGpu(tasks[i], servers[j]);

            vector<int> candidateTimes = getCandidateStartTimes(
                tasks[i],
                serverJobs[j]
            );

            for (int t = 0; t < candidateTimes.size(); t++) {
                int startTime = candidateTimes[t];

                if (!canPlaceAtTime(tasks[i], servers[j], serverJobs[j], startTime, useGpu)) {
                    continue;
                }

                int finishTime = startTime + tasks[i].runTime;

                int usedGpuNow, usedCpuNow, usedMemNow;
                getUsedResourceAtTime(
                    serverJobs[j],
                    startTime,
                    usedGpuNow,
                    usedCpuNow,
                    usedMemNow
                );

                int leftGpu = servers[j].gpu - usedGpuNow - useGpu;
                int leftCpu = servers[j].cpu - usedCpuNow - tasks[i].needCpu;
                int leftMem = servers[j].mem - usedMemNow - tasks[i].needMem;

                long long waitCost = 1LL * tasks[i].priority * (startTime - tasks[i].submitTime);
                long long finishCost = finishTime;
                long long gpuMemWaste = 1LL * useGpu * servers[j].gpuMem - tasks[i].needGpuMem;
                long long resourceWaste = 1LL * leftGpu * 1000 + 1LL * leftCpu * 10 + leftMem;

                long long score =
                    waitCost * 1000000LL +
                    finishCost * 10000LL +
                    gpuMemWaste * 100LL +
                    resourceWaste;

                if (score < bestScore) {
                    bestScore = score;
                    bestServer = j;
                    bestStartTime = startTime;
                    bestUseGpu = useGpu;
                }
            }
        }

        int finishTime = bestStartTime + tasks[i].runTime;

        ans[i] = {
            tasks[i].id,
            bestServer,
            bestStartTime,
            bestUseGpu,
            finishTime
        };

        RunningJob job;
        job.startTime = bestStartTime;
        job.finishTime = finishTime;
        job.useGpu = bestUseGpu;
        job.useCpu = tasks[i].needCpu;
        job.useMem = tasks[i].needMem;

        serverJobs[bestServer].push_back(job);
    }

    for (int i = 1; i <= N; i++) {
        cout << ans[i].taskId << " "
             << ans[i].serverId << " "
             << ans[i].startTime << " "
             << ans[i].useGpu << " "
             << ans[i].finishTime << "\n";
    }

    return 0;
}