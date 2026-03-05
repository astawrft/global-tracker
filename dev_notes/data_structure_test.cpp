// Early experiment with data structures (not used in final tracker)
// March 2026

#include <iostream>
#include <stack>
using namespace std;

int main() {
    stack<int> influenceScores;

    influenceScores.push(75);
    influenceScores.push(82);
    influenceScores.push(64);

    while (!influenceScores.empty()) {
        cout << "Score: " << influenceScores.top() << endl;
        influenceScores.pop();
    }

    return 0;
}
