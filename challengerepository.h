#ifndef CHALLENGEREPOSITORY_H
#define CHALLENGEREPOSITORY_H

#include <QString>
#include <QStringList>
#include <QVector>

struct AnswerRule
{
    QString spaceId;
    QString type;
    QStringList values;
};

struct Challenge
{
    QString id;
    QString title;
    QString prompt;
    QVector<AnswerRule> rules;
    int floor = 1;
};

class ChallengeRepository
{
public:
    bool loadFromJsonFile(const QString &filePath, QString *errorMessage = nullptr);
    void loadFallbackChallenges();

    const QVector<Challenge> &challenges() const;
    const Challenge *challengeById(const QString &id) const;
    QString sourceDescription() const;

private:
    QVector<Challenge> m_challenges;
    QString m_sourceDescription;
};

bool answerMatchesRule(const QString &answer, const AnswerRule &rule);
bool answersMatchChallenge(const QString &submittedText, const Challenge &challenge, QString *errorMessage = nullptr);

#endif // CHALLENGEREPOSITORY_H
