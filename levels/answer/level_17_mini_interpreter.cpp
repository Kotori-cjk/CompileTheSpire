#include <cctype>
#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <stack>
#include <string>
using namespace std;

//trim
string trim(const string& s) {
    // initial boundaries
    size_t left = 0, right = s.size();
    // skip leading spaces
    while (left < right && isspace(static_cast<unsigned char>(s[left]))) ++left;
    // skip trailing spaces
    while (right > left && isspace(static_cast<unsigned char>(s[right - 1]))) --right;
    // return trimmed slice
    return s.substr(left, right - left);
}

//operatorRules
int priorityOf(char op) {
    // + and - priority
    if (op == '+' || op == '-') return 1;
    // * priority
    if (op == '*') return 2;
    return 0;
}

int applyOp(int a, int b, char op) {
    // apply binary operator in a op b order
    if (op == '+') return a + b;
    if (op == '-') return a - b;
    return a * b;
}

//valueReader
int readValue(const string& expr, size_t& i, const map<string, int>& vars) {
    if (isdigit(static_cast<unsigned char>(expr[i]))) {
        // read integer literal
        int value = 0;
        while (i < expr.size() && isdigit(static_cast<unsigned char>(expr[i]))) {
            value = value * 10 + expr[i] - '0';
            ++i;
        }
        return value;
    }
    // read variable name
    string name;
    while (i < expr.size() && isalpha(static_cast<unsigned char>(expr[i]))) {
        name += expr[i++];
    }
    // look up variable value
    map<string, int>::const_iterator it = vars.find(name);
    return it == vars.end() ? 0 : it->second;
}

//expression
void reduceOnce(stack<int>& values, stack<char>& ops) {
    // pop two operands and one operator
    int b = values.top(); values.pop();
    int a = values.top(); values.pop();
    char op = ops.top(); ops.pop();
    values.push(applyOp(a, b, op));
}

int evaluate(string expr, const map<string, int>& vars) {
    stack<int> values;
    stack<char> ops;
    for (size_t i = 0; i < expr.size();) {
        if (isspace(static_cast<unsigned char>(expr[i]))) {
            ++i;
        } else if (isalnum(static_cast<unsigned char>(expr[i]))) {
            // push next value
            values.push(readValue(expr, i, vars));
        } else {
            char op = expr[i++];
            // reduce by operator priority, then push op
            while (!ops.empty() && priorityOf(ops.top()) >= priorityOf(op)) reduceOnce(values, ops);
            ops.push(op);
        }
    }
    // finish remaining operations
    while (!ops.empty()) reduceOnce(values, ops);
    return values.top();
}

//statementExecutor
void executeLine(const string& line, map<string, int>& vars) {
    if (line.substr(0, 3) == "let") {
        size_t eq = line.find('=');
        // parse variable name
        string name = trim(line.substr(3, eq - 3));
        // evaluate and store expression
        string expr = line.substr(eq + 1);
            vars[name] = evaluate(expr, vars);
    } else {
        // print expression value
        string expr = line.substr(6);
            cout << evaluate(expr, vars) << '\n';
    }
}

//main
int main() {
    int n;
    cin >> n;
    string line;
    // consume endline after n
    getline(cin, line);
    // variable table
    map<string, int> vars;
    // execute n lines
    for (int i = 0; i < n; ++i) {
        getline(cin, line);
        executeLine(line, vars);
    }
    return 0;
}
