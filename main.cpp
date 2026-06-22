#include <bits/stdc++.h>
using namespace std;

// 代码版本三：并发调度 + 任务排序优化

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

// 计算任务在某台服务器上至少需要多少张 GPU
int calcNeedGpu(const Task& task, const Server& server) {
    int gpuByMem = (task.needGpuMem + server.gpuMem - 1) / server.gpuMem;
    return max(task.needGpu, gpuByMem);
}

// 判断任务从整体资源上能不能在这台服务器运行
bool canRunOnServer(const Task& task, const Server& server) {
    int needGpu = calcNeedGpu(task, server);

    if (needGpu > server.gpu) return false;
    if (task.needCpu > server.cpu) return false;
    if (task.needMem > server.mem) return false;

    return true;
}

// 判断任务能不能在 startTime 时刻放入该服务器
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

// 寻找任务在某台服务器上的最早开始时间
int findEarliestStartTime(
    const Task& task,
    const Server& server,
    const vector<RunningJob>& jobs,
    int useGpu
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

    for (int i = 0; i < candidateTimes.size(); i++) {
        int startTime = candidateTimes[i];

        if (canPlaceAtTime(task, server, jobs, startTime, useGpu)) {
            return startTime;
        }
    }

    return candidateTimes.back();
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

    // 任务排序顺序
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

        int resourceA = tasks[a].needGpu + tasks[a].needCpu + tasks[a].needMem;
        int resourceB = tasks[b].needGpu + tasks[b].needCpu + tasks[b].needMem;

        return resourceA > resourceB;
    });

    vector<vector<RunningJob>> serverJobs(M + 1);
    vector<Answer> ans(N + 1);

    // 按排序后的顺序调度任务
    for (int idx = 0; idx < order.size(); idx++) {
        int i = order[idx];

        int bestServer = -1;
        int bestStartTime = 1e9;
        int bestUseGpu = 0;

        for (int j = 1; j <= M; j++) {
            if (!canRunOnServer(tasks[i], servers[j])) {
                continue;
            }

            int useGpu = calcNeedGpu(tasks[i], servers[j]);

            int startTime = findEarliestStartTime(
                tasks[i],
                servers[j],
                serverJobs[j],
                useGpu
            );

            if (startTime < bestStartTime) {
                bestStartTime = startTime;
                bestServer = j;
                bestUseGpu = useGpu;
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

    // 按任务编号输出答案
    for (int i = 1; i <= N; i++) {
        cout << ans[i].taskId << " "
             << ans[i].serverId << " "
             << ans[i].startTime << " "
             << ans[i].useGpu << " "
             << ans[i].finishTime << "\n";
    }

    return 0;
}