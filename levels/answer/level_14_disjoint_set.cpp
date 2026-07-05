#include <iostream>
#include <vector>
using namespace std;

class DisjointSet {
private:
    vector<int> parent, rankValue;

public:
    // constructor
    DisjointSet(int n = 0): parent(n + 1), rankValue(n + 1, 0) {
        // initialize each node as its own parent
        for (int i = 1; i <= n; ++i) parent[i] = i;
    }

    // find root
    int findRoot(int x) {
        // compress the path while finding the root
        if (parent[x] != x) parent[x] = findRoot(parent[x]);
        // return the representative
        return parent[x];
    }

    // merge two sets
    void unite(int a, int b) {
        // find representatives before merging
        int ra = findRoot(a), rb = findRoot(b);
        // union by rank
        if (ra == rb) return;
        if (rankValue[ra] < rankValue[rb]) parent[ra] = rb;
        else if (rankValue[ra] > rankValue[rb]) parent[rb] = ra;
        else {
            parent[rb] = ra;
            ++rankValue[ra];
        }
    }

    // query connectivity
    bool connected(int a, int b) {
        return findRoot(a) == findRoot(b);
    }
};

int main() {
    int n, m; cin >> n >> m;
    DisjointSet ds(n);
    for (int i = 0; i < m; ++i) { 
		char op; 
		int a, b; 
		cin >> op >> a >> b; 
		if (op == 'U') ds.unite(a, b); 
		else cout << (ds.connected(a, b) ? "YES" : "NO") << '\n'; 
		}
    return 0;
}
