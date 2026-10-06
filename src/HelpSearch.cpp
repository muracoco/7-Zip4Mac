// SPDX-License-Identifier: LGPL-3.0-or-later
#include "HelpSearch.h"
#include <QRegularExpression>
#include <QSet>
#include <memory>
#include <algorithm>
QStringList HelpSearch::words(const QString &text) {
    static const QRegularExpression word("[\\p{L}\\p{N}]+", QRegularExpression::UseUnicodePropertiesOption); QStringList result;
    auto matches = word.globalMatch(text.toCaseFolded()); while (matches.hasNext()) result << matches.next().captured(); return result;
}
namespace {
enum Kind { Term, Phrase, And, Or, Not, Near, Left, Right, End };
struct Token { Kind kind; QString text; };
struct Node { Kind kind; QString text; std::unique_ptr<Node> left, right; };
class Parser {
public:
    QString error;
    explicit Parser(QString query) {
        if (query.size() > 4096) { error = "The search expression is too long."; return; }
        for (int n = 0; n < query.size();) {
            if (query[n].isSpace()) { ++n; continue; }
            if (query[n] == '(' || query[n] == ')') { tokens << Token{query[n++] == '(' ? Left : Right, {}}; continue; }
            if (query[n] == '"') { const auto end = query.indexOf('"', ++n); if (end < 0) { error = "Missing closing quotation mark."; return; } tokens << Token{Phrase, query.mid(n, end - n)}; n = int(end + 1); continue; }
            int end = n; while (end < query.size() && !query[end].isSpace() && query[end] != '(' && query[end] != ')' && query[end] != '"') ++end;
            const auto value = query.mid(n, end - n); Kind kind = value == "AND" ? And : value == "OR" ? Or : value == "NOT" ? Not : value == "NEAR" ? Near : Term; tokens << Token{kind, value}; n = end;
        }
        if (tokens.size() > 128) error = "The search expression has too many terms."; tokens << Token{End, {}};
    }
    std::unique_ptr<Node> parse() { if (!error.isEmpty()) return {}; auto root = expression(0); if (peek() != End && error.isEmpty()) error = "Unexpected search operator or parenthesis."; return error.isEmpty() ? std::move(root) : nullptr; }
private:
    QList<Token> tokens; int position = 0;
    Kind peek() const { return tokens[position].kind; }
    std::unique_ptr<Node> expression(int depth) {
        if (depth > 32) { error = "Search parentheses are nested too deeply."; return {}; }
        auto lhs = conjunction(depth);
        while (error.isEmpty() && peek() == Or) { ++position; lhs = std::make_unique<Node>(Node{Or, {}, std::move(lhs), conjunction(depth)}); }
        return lhs;
    }
    std::unique_ptr<Node> conjunction(int depth) {
        auto lhs = proximity(depth);
        while (error.isEmpty() && (peek() == And || peek() == Not || peek() == Term || peek() == Phrase || peek() == Left)) {
            Kind kind = peek() == Not ? Not : And; if (peek() == And || peek() == Not) ++position;
            lhs = std::make_unique<Node>(Node{kind, {}, std::move(lhs), proximity(depth)});
        } return lhs;
    }
    std::unique_ptr<Node> proximity(int depth) {
        auto lhs = primary(depth); while (error.isEmpty() && peek() == Near) { ++position; lhs = std::make_unique<Node>(Node{Near, {}, std::move(lhs), primary(depth)}); } return lhs;
    }
    std::unique_ptr<Node> primary(int depth) {
        auto token = tokens[position++];
        if (token.kind == Left) { auto node = expression(depth + 1); if (error.isEmpty() && peek() == Right) ++position; else error = "Missing closing parenthesis."; return node; }
        if (token.kind != Term && token.kind != Phrase) { error = "Enter a word or phrase after the search operator."; --position; return {}; }
        if (token.text.trimmed().isEmpty()) { error = "Enter a nonempty word or phrase."; return {}; }
        return std::make_unique<Node>(Node{token.kind, token.text, {}, {}});
    }
};
struct Span { int first, last; };
struct Match { bool found = false; QList<Span> spans; };
void normalize(QList<Span> &spans) {
    std::sort(spans.begin(), spans.end(), [](const Span &a, const Span &b) { return a.first == b.first ? a.last < b.last : a.first < b.first; });
    spans.erase(std::unique(spans.begin(), spans.end(), [](const Span &a, const Span &b) { return a.first == b.first && a.last == b.last; }), spans.end());
}
struct Evaluation {
    QStringList text; bool similar; const QHash<QString, QString> &lemmas;
    QString stem(const QString &value) const { return lemmas.value(value, value); }
    Match evaluate(const Node *node) {
        if (node->kind == Term || node->kind == Phrase) {
            Match result; const auto query = HelpSearch::words(node->text);
            if (node->kind == Phrase || (query.size() > 1 && !node->text.contains('*') && !node->text.contains('?'))) {
                for (int n = 0; !query.isEmpty() && n + query.size() <= text.size(); ++n) { bool equal = true; for (int k = 0; k < query.size(); ++k) if (text[n + k] != query[k]) { equal = false; break; } if (equal) result.spans << Span{n, int(n + query.size() - 1)}; }
            } else {
                const bool wildcard = node->text.contains('*') || node->text.contains('?'); const auto value = !wildcard && query.size() == 1 ? query.first() : node->text.toCaseFolded(); QString pattern = QRegularExpression::escape(value); pattern.replace("\\*", ".*"); pattern.replace("\\?", "."); const QRegularExpression regex("^" + pattern + "$", QRegularExpression::UseUnicodePropertiesOption);
                for (int n = 0; n < text.size(); ++n) if (wildcard ? regex.match(text[n]).hasMatch() : text[n] == value || (similar && stem(text[n]) == stem(value))) result.spans << Span{n, n};
            } result.found = !result.spans.isEmpty(); return result;
        }
        auto lhs = evaluate(node->left.get()), rhs = evaluate(node->right.get());
        if (node->kind == Not) return rhs.found ? Match{} : lhs;
        if (node->kind == Near) {
            Match result; int longest = 0; for (const auto &span : rhs.spans) longest = qMax(longest, span.last - span.first);
            for (const auto &a : lhs.spans) {
                auto begin = std::lower_bound(rhs.spans.cbegin(), rhs.spans.cend(), a.first - longest - 9, [](const Span &span, int first) { return span.first < first; });
                for (auto b = begin; b != rhs.spans.cend() && b->first <= a.last + 9; ++b) {
                    const auto gap = qMax(a.first, b->first) - qMin(a.last, b->last) - 1;
                    if (gap <= 8 && !(a.first == b->first && a.last == b->last)) result.spans << Span{qMin(a.first, b->first), qMax(a.last, b->last)};
                }
            }
            normalize(result.spans); result.found = !result.spans.isEmpty(); return result;
        }
        const bool found = node->kind == Or ? lhs.found || rhs.found : lhs.found && rhs.found; if (!found) return {}; lhs.spans.append(rhs.spans); normalize(lhs.spans); lhs.found = true; return lhs;
    }
};
}
HelpSearch::Results HelpSearch::search(const QString &query, const QList<Topic> &topics, const Options &options) {
    Parser parser(query); const auto expression = parser.parse(); Results result; if (!expression) { result.error = parser.error; return result; }
    const QSet<QString> previous(options.previous.cbegin(), options.previous.cend());
    QSet<QString> vocabulary; if (options.similarWords) {
        const auto queryWords = words(query); vocabulary.unite(QSet<QString>(queryWords.cbegin(), queryWords.cend()));
        for (const auto &topic : topics) { if (options.withinPrevious && !previous.contains(topic.path)) continue; const auto tokens = words(options.titlesOnly ? topic.title : topic.text); vocabulary.unite(QSet<QString>(tokens.cbegin(), tokens.cend())); }
    }
    const auto lemmas = options.similarWords ? HelpSearch::lemmas(vocabulary.values()) : QHash<QString, QString>{};
    for (const auto &topic : topics) { if (options.withinPrevious && !previous.contains(topic.path)) continue; Evaluation evaluator{words(options.titlesOnly ? topic.title : topic.text), options.similarWords, lemmas}; const auto match = evaluator.evaluate(expression.get()); if (match.found) result.hits << Hit{topic.path, topic.title, int(match.spans.size())}; }
    std::stable_sort(result.hits.begin(), result.hits.end(), [](const Hit &a, const Hit &b) { return a.occurrences == b.occurrences ? a.title.compare(b.title, Qt::CaseInsensitive) < 0 : a.occurrences > b.occurrences; }); return result;
}
