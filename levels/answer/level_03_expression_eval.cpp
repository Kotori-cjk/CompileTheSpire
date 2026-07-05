#include <cctype>
#include <iostream>
#include <stack>
#include <string>
using namespace std;

//your priority_of_op
int priorityOf(char op) {
    if (op == '+' || op == '-') return 1;
    if (op == '*') return 2;
    return 0;
}


int applyOp(int a, int b, char op) {
    if (op == '+') return a + b;
    if (op == '-') return a - b;
    return a * b;
}

//your reduceOnce
void reduceOnce(stack<int>& values, stack<char>& ops) {
    int b = values.top(); values.pop();
    int a = values.top(); values.pop();
    char op = ops.top(); ops.pop();
    values.push(applyOp(a, b, op));
}

//your evaluate
int evaluate(const string& expr) {
    stack<int> values;
    stack<char> ops;
    for (size_t i = 0; i < expr.size();) {
        if (isspace(static_cast<unsigned char>(expr[i]))) {
            ++i;
        } else if (isdigit(static_cast<unsigned char>(expr[i]))) {
            int value = 0;
            while (i < expr.size() && isdigit(static_cast<unsigned char>(expr[i]))) {
                value = value * 10 + expr[i++] - '0';
            }
            values.push(value);
        } else if (expr[i] == '(') {
            ops.push(expr[i++]);
        } else if (expr[i] == ')') {
            while (!ops.empty() && ops.top() != '(') reduceOnce(values, ops);
            ops.pop();
            ++i;
        } else {
            char op = expr[i++];
            while (!ops.empty() && priorityOf(ops.top()) >= priorityOf(op)) {
                reduceOnce(values, ops);
            }
            ops.push(op);
        }
    }
    while (!ops.empty()) reduceOnce(values, ops);
    return values.top();
}

int main() {
    string expr;
    getline(cin, expr);
    cout << evaluate(expr) << '\n';
    return 0;
}

