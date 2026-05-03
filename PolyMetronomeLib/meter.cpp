#include "meter.h"

#include <QStringList>
#include <algorithm>
#include <numeric>

int Grouping::sum() const
{
    return std::accumulate(sizes.begin(), sizes.end(), 0);
}

bool Grouping::is_valid_for(int beats) const
{
    if (sizes.empty())
        return true;
    return sum() == beats;
}

QString Grouping::to_string() const
{
    QStringList parts;
    for (int s : sizes)
        parts << QString::number(s);
    return parts.join("+");
}

Grouping Grouping::parse(const QString& s, int beats, bool* ok)
{
    Grouping g;
    QString trimmed = s.trimmed();
    if (trimmed.isEmpty() || trimmed.compare("auto", Qt::CaseInsensitive) == 0) {
        if (ok)
            *ok = true;
        return g;
    }
    QString normalized = trimmed;
    normalized.replace(',', '+');
    normalized.replace(' ', '+');
    QStringList parts = normalized.split('+', Qt::SkipEmptyParts);
    for (const QString& p : parts) {
        bool part_ok = false;
        int v = p.trimmed().toInt(&part_ok);
        if (!part_ok || v <= 0) {
            if (ok)
                *ok = false;
            return Grouping{};
        }
        g.sizes.push_back(v);
    }
    if (g.sum() != beats) {
        if (ok)
            *ok = false;
        return Grouping{};
    }
    if (ok)
        *ok = true;
    return g;
}

QString MeasureSpec::time_signature_string() const
{
    return QString("%1/%2").arg(beats).arg(note_value);
}

bool MeasureSpec::operator==(const MeasureSpec& other) const
{
    return beats == other.beats && note_value == other.note_value && repeat == other.repeat && grouping.sizes == other.grouping.sizes;
}

int MeterSequence::total_measures() const
{
    int total = 0;
    for (const auto& m : measures)
        total += std::max(1, m.repeat);
    return total;
}

const MeasureSpec* MeterSequence::at_absolute(int absolute_idx) const
{
    if (absolute_idx < 0 || measures.empty())
        return nullptr;
    int cum = 0;
    for (const auto& m : measures) {
        int rep = std::max(1, m.repeat);
        if (absolute_idx < cum + rep)
            return &m;
        cum += rep;
    }
    return nullptr;
}

MeterSequence MeterSequence::default_4_4()
{
    MeterSequence s;
    s.measures.push_back(MeasureSpec{ 4, 4, 1, {} });
    return s;
}

QJsonObject MeasureSpec::to_json() const
{
    QJsonObject obj;
    obj["beats"] = beats;
    obj["note_value"] = note_value;
    obj["repeat"] = repeat;
    QJsonArray g;
    for (int s : grouping.sizes)
        g.append(s);
    obj["grouping"] = g;
    return obj;
}

MeasureSpec MeasureSpec::from_json(const QJsonObject& obj)
{
    MeasureSpec m;
    // Accept legacy "numerator"/"denominator" keys for older saved state.
    m.beats      = obj.contains("beats")      ? obj.value("beats").toInt(4)
                                              : obj.value("numerator").toInt(4);
    m.note_value = obj.contains("note_value") ? obj.value("note_value").toInt(4)
                                              : obj.value("denominator").toInt(4);
    m.repeat = obj.value("repeat").toInt(1);
    QJsonArray g = obj.value("grouping").toArray();
    for (const auto& v : g)
        m.grouping.sizes.push_back(v.toInt());
    return m;
}

QJsonArray MeterSequence::to_json() const
{
    QJsonArray arr;
    for (const auto& m : measures)
        arr.append(m.to_json());
    return arr;
}

MeterSequence MeterSequence::from_json(const QJsonArray& arr)
{
    MeterSequence seq;
    for (const auto& v : arr)
        seq.measures.push_back(MeasureSpec::from_json(v.toObject()));
    return seq;
}

const std::vector<Preset>& PresetLibrary::all()
{
    static const std::vector<Preset> presets = []() {
        std::vector<Preset> v;
        auto make = [](int n, int d, std::vector<int> g = {}, int rep = 1) {
            MeasureSpec m;
            m.beats = n;
            m.note_value = d;
            m.repeat = rep;
            m.grouping.sizes = std::move(g);
            return m;
        };
        v.push_back({ "4/4", MeterSequence({ make(4, 4) }) });
        v.push_back({ "3/4", MeterSequence({ make(3, 4) }) });
        v.push_back({ "6/8 (3+3)", MeterSequence({ make(6, 8, { 3, 3 }) }) });
        v.push_back({ "5/8 (3+2)", MeterSequence({ make(5, 8, { 3, 2 }) }) });
        v.push_back({ "5/8 (2+3)", MeterSequence({ make(5, 8, { 2, 3 }) }) });
        v.push_back({ "7/8 (2+2+3)", MeterSequence({ make(7, 8, { 2, 2, 3 }) }) });
        v.push_back({ "7/8 (3+2+2)", MeterSequence({ make(7, 8, { 3, 2, 2 }) }) });
        v.push_back({ "Take Five (5/4)", MeterSequence({ make(5, 4) }) });
        v.push_back({ "9/8 (3+3+3)", MeterSequence({ make(9, 8, { 3, 3, 3 }) }) });
        v.push_back({ "11/8 (3+3+3+2)", MeterSequence({ make(11, 8, { 3, 3, 3, 2 }) }) });
        return v;
    }();
    return presets;
}
