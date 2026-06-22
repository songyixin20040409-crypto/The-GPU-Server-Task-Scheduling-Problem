#include <bits/stdc++.h>
using namespace std;
//代码版本一 一台服务器同一时间运行一个任务
struct Server {
    int id;
    int gpu;
    int gpuMem; // 单张GPU显存
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


struct Answer {
    int taskId;
    int serverId;
    int startTime;
    int useGpu;
    int finishTime;
};

// 一个任务最终需要的 GPU 数量 =任务“显式要求的 GPU 数量”和“由显存需求换算出来的 GPU 数量”两者中的最大值
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
    // GPU数量够不够
    // CPU够不够
    // 内存够不够
    // 显存能不能被这些GPU满足
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



    vector<int> serverEndTime(M + 1, 0);
    vector<Answer> ans(N + 1);

    for (int i = 1; i <= N; i++) {
        int bestServer = -1;
        int bestStartTime = 1e9;
        int bestUseGpu = 0;

        for (int j = 1; j <= M; j++) {
            if (!canRunOnServer(tasks[i], servers[j])) {
                continue;
            }

            int useGpu = calcNeedGpu(tasks[i], servers[j]);
            int startTime = max(tasks[i].submitTime, serverEndTime[j]);

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

        serverEndTime[bestServer] = finishTime;
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


//遍历每一个任务，每个任务都尝试所有服务器，计算最优开始时间、最优服务器、GPU使用量和完成时间，最后用 ans 数组存储每个任务的调度结果
