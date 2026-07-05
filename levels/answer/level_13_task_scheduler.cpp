#include <iostream>
#include <queue>
#include <string>
#include <vector>
using namespace std;

//your task
struct Task {
    // task name
    string name;
    // higher value means higher priority
    int priority;
    // earlier time wins when priority ties
    int time;
};

//task_compare
struct TaskCompare {
    bool operator()(const Task& a, const Task& b) const {
        // compare priority first
        if (a.priority != b.priority) return a.priority < b.priority;
        // tie-break by earlier time
        return a.time > b.time;
    }
};

//SchedulerLoader
void loadTasks(int n, priority_queue<Task, vector<Task>, TaskCompare>& pq) {
    for (int i = 0; i < n; ++i) {
        // read one task and push it into pq
        Task task;
        cin >> task.name >> task.priority >> task.time;
        pq.push(task);
    }
}

//SchedulerRunner
void runTasks(priority_queue<Task, vector<Task>, TaskCompare>& pq) {
    while (!pq.empty()) {
        // take the scheduled task
        Task task = pq.top();
        pq.pop();
        // print its name and spacing
        cout << task.name;
        if (!pq.empty()) cout << ' ';
    }
    cout << '\n';
}

int main() {
    int n; cin >> n;
    priority_queue<Task, vector<Task>, TaskCompare> pq;
    loadTasks(n, pq);
    runTasks(pq);
    return 0;
}
