#ifndef AL_KIT_SEARCH_H
#define AL_KIT_SEARCH_H
#include <string>
#include <vector>
class ALKitRow;
class ALKitSearch
{
public: struct Hit{std::string page_key,card_id,widget,setting;S32 option=-1;ALKitRow* row=nullptr;};using Hits=std::vector<Hit>;
 void setQuery(const std::string& q){mQuery=q;}const std::string& queryText()const{return mQuery;}Hits query(const std::string&)const{return {};}
private:std::string mQuery;
};
#endif
