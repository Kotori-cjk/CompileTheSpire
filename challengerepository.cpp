#include "challengerepository.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace {

QStringList valuesFromJson(const QJsonValue &value)
{
    QStringList values;
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        for (const QJsonValue &entry : array) {
            values.append(entry.toString().trimmed());
        }
    } else if (value.isString()) {
        values.append(value.toString().trimmed());
    }
    values.removeAll("");
    return values;
}

QVector<AnswerRule> rulesFromSpaces(const QJsonObject &spaces)
{
    QVector<AnswerRule> rules;
    const QStringList keys = spaces.keys();
    for (const QString &spaceId : keys) {
        const QJsonObject ruleObject = spaces.value(spaceId).toObject();
        AnswerRule rule;
        rule.spaceId = spaceId;
        rule.type = ruleObject.value("type").toString("match").trimmed();
        rule.values = valuesFromJson(ruleObject.value("val"));
        rules.append(rule);
    }
    return rules;
}

QString promptForCode(const QString &title, const QString &code, const QVector<AnswerRule> &rules)
{
    QStringList blanks;
    for (const AnswerRule &rule : rules) {
        blanks.append(rule.spaceId);
    }

    return QString("%1\n\n%2\n\nFill blank(s): %3\n"
                   "For multiple blanks, submit answers separated by semicolons in this order.")
        .arg(title, code, blanks.join(", "));
}

} // namespace

bool ChallengeRepository::loadFromJsonFile(const QString &filePath, QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QString("Cannot open %1").arg(filePath);
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = QString("Invalid challenge JSON: %1").arg(parseError.errorString());
        }
        return false;
    }

    QVector<Challenge> loadedChallenges;
    const QJsonObject root = document.object();

    const QJsonObject monsters = root.value("monsters").toObject();
    const QStringList monsterIds = monsters.keys();
    for (int index = 0; index < monsterIds.size(); ++index) {
        const QString &monsterId = monsterIds.at(index);
        const QJsonObject monster = monsters.value(monsterId).toObject();
        const QVector<AnswerRule> rules = rulesFromSpaces(monster.value("spaces").toObject());
        Challenge challenge;
        challenge.id = monsterId;
        challenge.title = monster.value("nickname").toString(monster.value("name").toString(monsterId));
        challenge.prompt = promptForCode(challenge.title, monster.value("code").toString(), rules);
        challenge.rules = rules;
        challenge.floor = index + 1;
        loadedChallenges.append(challenge);
    }

    if (root.contains("boss")) {
        const QJsonObject boss = root.value("boss").toObject();
        const QVector<AnswerRule> rules = rulesFromSpaces(boss.value("spaces").toObject());
        Challenge challenge;
        challenge.id = "boss";
        challenge.title = "Boss";
        challenge.prompt = promptForCode(challenge.title, boss.value("code").toString(), rules);
        if (boss.contains("input") || boss.contains("output")) {
            challenge.prompt += QString("\n\nInput: %1\nExpected output: %2")
                                    .arg(boss.value("input").toString(), boss.value("output").toString());
        }
        challenge.rules = rules;
        challenge.floor = loadedChallenges.size() + 1;
        loadedChallenges.append(challenge);
    }

    if (loadedChallenges.isEmpty()) {
        if (errorMessage) {
            *errorMessage = "No monsters or boss challenges were found.";
        }
        return false;
    }

    m_challenges = loadedChallenges;
    m_sourceDescription = filePath;
    return true;
}

void ChallengeRepository::loadFallbackChallenges()
{
    m_challenges = {
        {
            "runtime_bug",
            "Runtime Bug",
            "Fill the blank:\n\n"
            "int total = 0;\n"
            "for (int i = 1; i <= 3; ++i) {\n"
            "    total += i;\n"
            "}\n\n"
            "// total == ____",
            {{"space1", "match", {"6"}}},
            1,
        },
        {
            "memory_leak",
            "Memory Leak",
            "Fill the blank:\n\n"
            "std::vector<int> nums = {2, 4, 6};\n"
            "int count = nums.____();\n\n"
            "// count == 3",
            {{"space1", "match", {"size"}}},
            2,
        },
        {
            "linker_error",
            "Linker Error",
            "Fill the blank:\n\n"
            "class Solver {\n"
            "public:\n"
            "    ____ int answer() { return 42; }\n"
            "};\n\n"
            "// answer() can be called without an object",
            {{"space1", "match", {"static"}}},
            3,
        },
    };
    m_sourceDescription = "built-in fallback";
}

const QVector<Challenge> &ChallengeRepository::challenges() const
{
    return m_challenges;
}

const Challenge *ChallengeRepository::challengeById(const QString &id) const
{
    for (const Challenge &challenge : m_challenges) {
        if (challenge.id == id) {
            return &challenge;
        }
    }
    return nullptr;
}

QString ChallengeRepository::sourceDescription() const
{
    return m_sourceDescription;
}

bool answerMatchesRule(const QString &answer, const AnswerRule &rule)
{
    const QString submitted = answer.trimmed();
    if (submitted.isEmpty()) {
        return false;
    }

    if (rule.type == "prefix") {
        for (const QString &value : rule.values) {
            if (submitted.startsWith(value)) {
                return true;
            }
        }
        return false;
    }

    if (rule.type == "find") {
        for (const QString &value : rule.values) {
            if (submitted.contains(value)) {
                return true;
            }
        }
        return false;
    }

    if (rule.type == "regex") {
        for (const QString &value : rule.values) {
            const QRegularExpression regex(value);
            if (regex.isValid() && regex.match(submitted).hasMatch()) {
                return true;
            }
        }
        return false;
    }

    // "match" accepts an array now; any exact option is enough to pass.
    for (const QString &value : rule.values) {
        if (submitted == value) {
            return true;
        }
    }
    return false;
}

bool answersMatchChallenge(const QString &submittedText, const Challenge &challenge, QString *errorMessage)
{
    const QStringList answers = submittedText.split(';', Qt::SkipEmptyParts);
    if (answers.size() != challenge.rules.size()) {
        if (errorMessage) {
            *errorMessage = QString("Expected %1 answer(s), separated by semicolons.")
                                .arg(challenge.rules.size());
        }
        return false;
    }

    for (int i = 0; i < challenge.rules.size(); ++i) {
        if (!answerMatchesRule(answers.at(i), challenge.rules.at(i))) {
            if (errorMessage) {
                *errorMessage = QString("Blank %1 did not match.").arg(challenge.rules.at(i).spaceId);
            }
            return false;
        }
    }

    return true;
}
