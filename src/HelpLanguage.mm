// SPDX-License-Identifier: LGPL-3.0-or-later
#include "HelpSearch.h"
#import <NaturalLanguage/NaturalLanguage.h>
QString HelpSearch::lemma(const QString &word) {
    return lemmas({word}).value(word, word.toCaseFolded());
}
QHash<QString, QString> HelpSearch::lemmas(const QStringList &words) {
    QHash<QString, QString> result;
    @autoreleasepool {
        const auto bytes = words.join(' ').toUtf8(); NSString *text = [[NSString alloc] initWithBytes:bytes.constData() length:bytes.size() encoding:NSUTF8StringEncoding];
        NLTagger *tagger = [[NLTagger alloc] initWithTagSchemes:@[NLTagSchemeLemma]]; tagger.string = text; [tagger setLanguage:NLLanguageEnglish range:NSMakeRange(0, text.length)];
        NSUInteger offset = 0;
        for (const auto &word : words) {
            NSString *tag = [tagger tagAtIndex:offset unit:NLTokenUnitWord scheme:NLTagSchemeLemma tokenRange:nullptr];
            result.insert(word, tag ? QString::fromUtf8(tag.UTF8String).toCaseFolded() : word.toCaseFolded());
            offset += word.size() + 1;
        }
    }
    return result;
}
