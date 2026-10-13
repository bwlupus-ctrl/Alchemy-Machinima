#ifndef AL_KIT_SEARCH_H
#define AL_KIT_SEARCH_H

#include "stdtypes.h"
#include "lltimer.h"

#include <string>
#include <unordered_map>
#include <vector>

class ALKitRow;

class ALKitSearch
{
public:
    struct Hit
    {
        std::string group_key;
        std::string group_label;
        std::string page_key;
        std::string page_label;
        std::string card_id;
        std::string card_label;
        std::string widget;
        std::string setting;
        std::string label;
        std::string label_lower;
        std::string path;
        std::string external_floater;
        std::string external_label;
        std::string external_tab;
        std::string external_tab_label;
        S32         option   = -1;
        S32         decimals = 3;
        S32         score    = 0;
        U32         order    = 0;
        bool        advanced = false;
        bool        external = false;
        ALKitRow*   row      = nullptr;
    };
    using Hits = std::vector<Hit>;

    bool               load(const std::string& index_file);
    bool               tokenizeBatch(U32 count = 400);
    bool               loading() const;
    void               setCurrentPage(const std::string& page_key);
    void               setQuery(const std::string& query);
    const std::string& queryText() const;
    Hits               query(const std::string& query) const;
    Hits               all() const;
    void               clear();

private:
    struct Entry
    {
        std::string      group_key;
        std::string      group_label;
        std::string      page_key;
        std::string      page_label;
        std::string      card_id;
        std::string      card_label;
        std::string      widget;
        std::string      setting;
        std::string      label;
        std::string      label_lower;
        std::string      path;
        S32              option   = -1;
        S32              decimals = 3;
        U32              order    = 0;
        bool             advanced = false;
        bool             external = false;
        std::vector<U32> label_tokens;
        std::vector<U32> card_tokens;
        std::vector<U32> keyword_tokens;
        std::vector<U32> help_tokens;
        std::vector<U32> setting_tokens;
        std::vector<U32> widget_tokens;
        std::vector<U32> synonym_tokens;
        std::string      external_floater;
        std::string      external_label;
        std::string      external_tab;
        std::string      external_tab_label;
    };

    U32         intern(const std::string& token);
    void        tokenize(const std::string& value, std::vector<U32>& out);
    static void splitTokens(const std::string& value, std::vector<std::string>& out,
                            bool lowercase_tokens = true);

    std::string                           mQuery;
    std::string                           mCurrentPage;
    std::vector<std::vector<std::string>> mPending;
    U32                                   mPendingIndex = 0;
    std::vector<Entry>                    mEntries;
    std::unordered_map<std::string, U32>  mTokenIds;
    std::vector<std::string>              mTokens;
    LLTimer                               mLoadTimer;
};

#endif
