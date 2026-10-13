#include "llviewerprecompiledheaders.h"

#include "alkitsearch.h"
#include "aldirectorqol.h"

#include "lldir.h"
#include "llfile.h"
#include "llstring.h"
#include "aluiperfselftest.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace
{
constexpr S32 INDEX_FIELDS = 22;

std::string unescapeField(const std::string& value)
{
    std::string result;
    result.reserve(value.size());
    bool escaped = false;
    for (char ch : value)
    {
        if (escaped)
        {
            result.push_back(ch == 't' ? '\t' : ch == 'n' ? '\n' : ch);
            escaped = false;
        }
        else if (ch == '\\')
        {
            escaped = true;
        }
        else
        {
            result.push_back(ch);
        }
    }
    if (escaped)
    {
        result.push_back('\\');
    }
    return result;
}

void splitFields(const std::string& line, std::vector<std::string>& fields)
{
    fields.clear();
    std::string field;
    for (char ch : line)
    {
        if (ch == '\t')
        {
            fields.push_back(unescapeField(field));
            field.clear();
        }
        else
        {
            field.push_back(ch);
        }
    }
    fields.push_back(unescapeField(field));
}
} // namespace

bool ALKitSearch::load(const std::string& index_file)
{
    clear();
    mLoadTimer.reset();
    const std::string filename = gDirUtilp->findSkinnedFilename(LLDir::XUI, index_file);
    llifstream        input(filename.c_str(), std::ios::in | std::ios::binary);
    if (!input)
    {
        LL_WARNS("DirectorKit") << "KitSearch cannot open " << filename << LL_ENDL;
        return false;
    }
    std::ostringstream bytes;
    bytes << input.rdbuf();
    const std::string xml = bytes.str();
    if (xml.find("<director_kit_index version=\"3\" fields=\"22\">") == std::string::npos)
    {
        LL_WARNS("DirectorKit") << "KitSearch rejected index header in " << filename << LL_ENDL;
        return false;
    }
    const size_t begin = xml.find("<![CDATA[");
    const size_t end   = xml.rfind("]]>");
    if (begin == std::string::npos || end == std::string::npos || end < begin + 9)
    {
        LL_WARNS("DirectorKit") << "KitSearch rejected missing CDATA in " << filename << LL_ENDL;
        return false;
    }
    std::istringstream       lines(xml.substr(begin + 9, end - begin - 9));
    std::string              line;
    std::vector<std::string> fields;
    while (std::getline(lines, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (line.empty())
        {
            continue;
        }
        splitFields(line, fields);
        if (fields.size() != INDEX_FIELDS)
        {
            LL_WARNS("DirectorKit") << "KitSearch rejected index line with " << fields.size() << " fields" << LL_ENDL;
            clear();
            return false;
        }
        mPending.push_back(fields);
    }
    return true;
}

bool ALKitSearch::tokenizeBatch(U32 count)
{
    const U32 end = llmin((U32)mPending.size(), mPendingIndex + count);
    for (; mPendingIndex < end; ++mPendingIndex)
    {
        const std::vector<std::string>& fields = mPending[mPendingIndex];
        Entry                           entry;
        entry.group_key   = fields[0];
        entry.page_key    = fields[0] + "/" + fields[1];
        entry.card_id     = fields[5];
        entry.option      = fields[7].empty() ? -1 : atoi(fields[7].c_str());
        entry.widget      = fields[8];
        entry.label       = fields[9];
        entry.label_lower = entry.label;
        LLStringUtil::toLower(entry.label_lower);
        entry.setting  = fields[11];
        entry.decimals = atoi(fields[12].c_str());
        entry.advanced = fields[13].find('a') != std::string::npos;
        entry.external = fields[13].find('e') != std::string::npos;
        entry.external_floater   = fields[17];
        entry.external_label     = fields[18];
        entry.external_tab       = fields[19];
        entry.external_tab_label = fields[20];
        entry.order    = mPendingIndex;
        entry.path     = entry.external ? fields[9]
                                        : fields[3] + " \xE2\x80\xBA " + fields[2] + " \xE2\x80\xBA " + fields[6] + " \xC2\xB7 " + fields[9];
        tokenize(fields[9], entry.label_tokens);
        tokenize(fields[6], entry.card_tokens);
        tokenize(fields[14], entry.keyword_tokens);
        tokenize(fields[15], entry.help_tokens);
        tokenize(fields[11], entry.setting_tokens);
        tokenize(fields[8], entry.widget_tokens);
        tokenize(fields[16], entry.synonym_tokens);
        mEntries.push_back(std::move(entry));
    }
    if (mPendingIndex == mPending.size())
    {
        LL_INFOS("DirectorKit") << "KitSearch index " << mEntries.size() << " entries " << mLoadTimer.getElapsedTimeF64() * 1000. << " ms"
                                << LL_ENDL;
        mPending.clear();
        return false;
    }
    return true;
}

bool ALKitSearch::loading() const
{ return !mPending.empty(); }

void ALKitSearch::setCurrentPage(const std::string& page_key)
{ mCurrentPage = page_key; }

void ALKitSearch::setQuery(const std::string& query)
{ mQuery = query; }

const std::string& ALKitSearch::queryText() const
{ return mQuery; }

ALKitSearch::Hits ALKitSearch::query(const std::string& query_text) const
{
    LLTimer                  query_timer;
    std::vector<std::string> query_tokens;
    splitTokens(query_text, query_tokens);
    Hits hits;
    if (query_tokens.empty())
    {
        return hits;
    }
    for (const Entry& entry : mEntries)
    {
        S32  total   = 0;
        bool matches = true;
        for (const std::string& query_token : query_tokens)
        {
            S32  best  = 0;
            auto score = [this, &query_token, &best](const std::vector<U32>& tokens, ALDirectorQol::SearchField field)
            {
                for (U32 id : tokens)
                {
                    const S32 token_score = ALDirectorQol::searchTokenScore(query_token, mTokens[id], field);
                    if (token_score)
                        best = llmax(best, token_score);
                }
            };
            score(entry.label_tokens, ALDirectorQol::SearchField::LABEL);
            score(entry.card_tokens, ALDirectorQol::SearchField::CARD);
            score(entry.keyword_tokens, ALDirectorQol::SearchField::KEYWORD);
            if (query_token.size() > 2)
            {
                score(entry.help_tokens, ALDirectorQol::SearchField::HELP);
                score(entry.setting_tokens, ALDirectorQol::SearchField::SETTING);
                score(entry.widget_tokens, ALDirectorQol::SearchField::WIDGET);
                score(entry.synonym_tokens, ALDirectorQol::SearchField::SYNONYM);
            }
            const S32 full_label_score = ALDirectorQol::fuzzyTokenScore(query_token, entry.label_lower);
            if (full_label_score)
                best = llmax(best, 100 + full_label_score);
            if (!best)
            {
                matches = false;
                break;
            }
            total += best;
        }
        if (matches)
        {
            Hit hit;
            hit.group_key = entry.group_key;
            hit.page_key  = entry.page_key;
            hit.card_id   = entry.card_id;
            hit.widget    = entry.widget;
            hit.setting   = entry.setting;
            hit.label     = entry.label;
            hit.path      = entry.path;
            hit.option    = entry.option;
            hit.decimals  = entry.decimals;
            hit.score     = total;
            hit.order     = entry.order;
            hit.advanced  = entry.advanced;
            hit.external = entry.external;
            hit.external_floater = entry.external_floater;
            hit.external_label = entry.external_label;
            hit.external_tab = entry.external_tab;
            hit.external_tab_label = entry.external_tab_label;
            hits.push_back(std::move(hit));
        }
    }
    std::stable_sort(hits.begin(), hits.end(),
                     [this](const Hit& lhs, const Hit& rhs)
                     {
                         if (lhs.score != rhs.score || lhs.external != rhs.external)
                             return ALDirectorQol::searchResultBefore(lhs.score, lhs.external, lhs.order,
                                                                      rhs.score, rhs.external, rhs.order);
                         const bool lhs_current = lhs.page_key == mCurrentPage;
                         const bool rhs_current = rhs.page_key == mCurrentPage;
                         return lhs_current != rhs_current ? lhs_current : lhs.order < rhs.order;
                     });
    if (ALUIPerfSelfTest::isActive())
    {
        LL_INFOS("DirectorKit") << "KitSearch query \"" << query_text << "\" " << query_timer.getElapsedTimeF64() * 1000. << " ms "
                                << hits.size() << LL_ENDL;
    }
    return hits;
}

ALKitSearch::Hits ALKitSearch::all() const
{
    Hits hits;
    hits.reserve(mEntries.size());
    for (const Entry& entry : mEntries)
    {
        Hit hit;
        hit.group_key = entry.group_key;
        hit.page_key = entry.page_key;
        hit.card_id = entry.card_id;
        hit.widget = entry.widget;
        hit.setting = entry.setting;
        hit.label = entry.label;
        hit.path = entry.path;
        hit.option = entry.option;
        hit.decimals = entry.decimals;
        hit.order = entry.order;
        hit.advanced = entry.advanced;
        hit.external = entry.external;
        hit.external_floater = entry.external_floater;
        hit.external_label = entry.external_label;
        hit.external_tab = entry.external_tab;
        hit.external_tab_label = entry.external_tab_label;
        hits.push_back(std::move(hit));
    }
    return hits;
}

void ALKitSearch::clear()
{
    mQuery.clear();
    mPending.clear();
    mPendingIndex = 0;
    mEntries.clear();
    mTokenIds.clear();
    mTokens.clear();
}

U32 ALKitSearch::intern(const std::string& token)
{
    auto found = mTokenIds.find(token);
    if (found != mTokenIds.end())
    {
        return found->second;
    }
    const U32 id = (U32)mTokens.size();
    mTokens.push_back(token);
    mTokenIds.emplace(token, id);
    return id;
}

void ALKitSearch::tokenize(const std::string& value, std::vector<U32>& out)
{
    std::vector<std::string> tokens;
    splitTokens(value, tokens);
    for (const std::string& token : tokens)
    {
        out.push_back(intern(token));
    }
}

void ALKitSearch::splitTokens(const std::string& value, std::vector<std::string>& out)
{
    std::string token;
    char        previous = 0;
    for (char ch : value)
    {
        const unsigned char byte = (unsigned char)ch;
        if (std::isalnum(byte))
        {
            if (!token.empty() && std::isupper(byte) && std::islower((unsigned char)previous))
            {
                LLStringUtil::toLower(token);
                out.push_back(token);
                token.clear();
            }
            token.push_back(ch);
        }
        else if (!token.empty())
        {
            LLStringUtil::toLower(token);
            out.push_back(token);
            token.clear();
        }
        previous = ch;
    }
    if (!token.empty())
    {
        LLStringUtil::toLower(token);
        out.push_back(token);
    }
}
