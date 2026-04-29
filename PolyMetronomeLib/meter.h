#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <vector>

struct Grouping
{
    std::vector<int> sizes;

    bool is_empty() const { return sizes.empty(); }
    int sum() const;
    bool is_valid_for(int numerator) const;
    QString to_string() const;
    static Grouping parse(const QString& s, int numerator, bool* ok = nullptr);
};

struct MeasureSpec
{
    int numerator = 4;
    int denominator = 4;
    int repeat = 1;
    Grouping grouping;

    QString time_signature_string() const;
    bool operator==(const MeasureSpec& other) const;

    QJsonObject to_json() const;
    static MeasureSpec from_json(const QJsonObject& obj);
};

class MeterSequence
{
public:
    std::vector<MeasureSpec> measures;

    MeterSequence() = default;
    explicit MeterSequence(std::vector<MeasureSpec> ms) : measures(std::move(ms)) {}

    int total_measures() const;
    const MeasureSpec* at_absolute(int absolute_idx) const;
    bool empty() const { return measures.empty(); }

    QJsonArray to_json() const;
    static MeterSequence from_json(const QJsonArray& arr);

    static MeterSequence default_4_4();
};

struct Preset
{
    QString name;
    MeterSequence sequence;
};

class PresetLibrary
{
public:
    static const std::vector<Preset>& all();
};
