#include <iostream>
#include <queue>
#include <string>
#include <vector>
using namespace std;

//Point and Inside
struct Point {
    // row and column
    int row, col;
};

bool inside(int r, int c, int n, int m) {
    // check grid bounds
    return 0 <= r && r < n && 0 <= c && c < m;
}

//BfsStart
int shortestPath(const vector<string>& grid, Point start, Point target) {
    int n = grid.size(), m = grid[0].size();
    vector<vector<int> > dist(n, vector<int>(m, -1));
    queue<Point> q;
    // enqueue start
    q.push(start);
    // mark start distance
    dist[start.row][start.col] = 0;
    // four movement directions
    int dr[] = {-1, 1, 0, 0};
    int dc[] = {0, 0, -1, 1};
    while (!q.empty()) {
        Point cur = q.front();
        q.pop();
        if (cur.row == target.row && cur.col == target.col) return dist[cur.row][cur.col];
        // expand neighbors
        for (int k = 0; k < 4; ++k) {
            int nr = cur.row + dr[k], nc = cur.col + dc[k];
            // can step onto this cell?
            if (inside(nr, nc, n, m) && grid[nr][nc] != '#' && dist[nr][nc] == -1) {
                // record distance and push
                dist[nr][nc] = dist[cur.row][cur.col] + 1;
                q.push(Point{nr, nc});
            }
        }
    }
    // unreachable case
    return -1;
}

int main() {
    int n, m; cin >> n >> m;
    vector<string> grid(n);
    Point start{-1, -1}, target{-1, -1};
    for (int i = 0; i < n; ++i) {
        // read one map row
        cin >> grid[i];
        for (int j = 0; j < m; ++j) {
            // locate S
            if (grid[i][j] == 'S') start = Point{i, j};
            // locate T
            if (grid[i][j] == 'T') target = Point{i, j};
        }
    }
    cout << shortestPath(grid, start, target) << '\n';
    return 0;
}
