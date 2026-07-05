#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <vector>
using namespace std;

//recordstruct
struct Record {
    // student name
    string student;
    // course score
    int score;
};

//course
class Course {
private:
    string name;
    vector<Record> records;

public:
    // constructor
    Course(string courseName = ""): name(courseName) {}

    void addScore(const string& student, int score) {
        // append a score record
        records.push_back(Record{student, score});
    }

    // ranking function
    vector<Record> ranking() const {
        // copy records before sorting
        vector<Record> result = records;
        // sort by score desc, then name asc
        sort(result.begin(), result.end(), [](const Record& a, const Record& b) {
            if (a.score != b.score) return a.score > b.score;
            return a.student < b.student;
        });
        // return sorted copy
        return result;
    }
};

//addRecord
void addRecord(map<string, Course>& courses, const string& student, const string& course, int score) {
    // create course if needed
    if (courses.find(course) == courses.end()) courses[course] = Course(course);
    // add score to selected course
    courses[course].addScore(student, score);
}

int main() {
    int n; cin >> n;
    map<string, Course> courses;
    for (int i = 0; i < n; ++i) { 
		string student, course; 
		int score; 
		cin >> student >> course >> score; 
		addRecord(courses, student, course, score); 
		}
    string target; 
	cin >> target;
    vector<Record> rank = courses[target].ranking();
    for (size_t i = 0; i < rank.size(); ++i) cout << i + 1 << ' ' << rank[i].student << ' ' << rank[i].score << '\n';
    return 0;
}
