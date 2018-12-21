#ifndef __main_h__
#define __main_h__

#include <mysql.h>
#include <curl/curl.h>

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <sstream>
#include <chrono>
#include <thread>
#include <mutex>

#include "json.hpp"
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <sys/time.h>

using namespace std ;
using json = nlohmann::json;

#define DEBUG_OUTPUT 0

#define NS_UNKNOWN -999
#define UNKNOWN_WIKIDATA_ITEM 0
#define DB_PAGE_BATCH_SIZE 1000000
#define NS_FILE 6
#define MAX_QUERY_OUTPUT_LENGTH 2000
#define MAX_HTML_RESULTS 10000

class TPlatform ;
class TPageList ;

extern vector <string> file_data_keys ;
extern TPlatform *root_platform ;
extern std::mutex g_root_platform_mutex;

string ltrim(std::string s) ;
string rtrim(string s) ;
string trim(string s) ;
char *loadFileFromDisk ( string filename ) ;
string loadAndCacheFileFromDisk ( string filename ) ;
void split ( const string &input , vector <string> &v , char delim , uint32_t max = 0 ) ;
const std::string urlencode( const std::string& s ) ;
const std::string urldecode ( const std::string& str ) ;
string getWikiServer ( string wiki ) ;
string loadTextfromURL ( string url ) ;
bool loadJSONfromURL ( string url , json &j , bool use_cache = false ) ;
bool loadJSONfromPOST ( string url , const string &post , json &j ) ;
void stringReplace(std::string& str, string oldStr, string newStr) ;
string space2_ ( string s ) ;
string _2space ( string s ) ;
string ui2s ( uint32_t i ) ;
string f2s ( float f ) ;
string escapeURLcomponent ( string s ) ;
double time_diff(struct timeval x , struct timeval y);
string pad ( string s , int num , char c ) ;


class TWikidataDB {
public:
	TWikidataDB () {} ;
	TWikidataDB ( string wiki , TPlatform *_platform = NULL ) ;
	void setHostDB ( string host , string db ) ;
	void doConnect ( bool first = false ) ;
	void runQuery ( string sql ) ;
	bool isConnected() ;
	MYSQL_RES *getQueryResults ( string sql ) ;
	string escape ( string s ) ;
	string space2_ ( string s ) ;
	uint32_t lastInsertID () ;
	~TWikidataDB () ;
	
protected:
	MYSQL mysql;
	string _host , _config_file , _database , _wiki ;
	TPlatform *platform = NULL ;
	bool did_connect = false ;
	
	void finishWithError ( string msg = "" ) ;
	bool setHostDBFromWiki ( string wiki ) ;

	struct MemoryStruct {
	  char *memory;
	  size_t size;
	};

	static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
	  size_t realsize = size * nmemb;
	  struct MemoryStruct *mem = (struct MemoryStruct *)userp;
 
	  mem->memory = (char*) realloc(mem->memory, mem->size + realsize + 1);
	  if(mem->memory == NULL) {
		/* out of memory! */ 
		printf("not enough memory (realloc returned NULL)\n");
		return 0;
	  }
 
	  memcpy(&(mem->memory[mem->size]), contents, realsize);
	  mem->size += realsize;
	  mem->memory[mem->size] = 0;
 
	  return realsize;
	}
 
} ;


struct CURLMemoryStruct {
  char *memory;
  size_t size;
};


static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  struct CURLMemoryStruct *mem = (struct CURLMemoryStruct *)userp;

  mem->memory = (char*) realloc(mem->memory, mem->size + realsize + 1);
  if(mem->memory == NULL) {
	/* out of memory! */ 
	printf("not enough memory (realloc returned NULL)\n");
	return 0;
  }

  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}


class TPageMetadata {
public:
	string getMisc ( const string &key , const string &_default = "" ) ;

	uint32_t id = 0 ;
	uint32_t size = 0 ;
	int16_t ns = NS_UNKNOWN ;
	bool is_full_title = true ;
	uint32_t q = 0 ;
	string timestamp ;
	map <string,string> misc ;
} ;

class TPage {
public:
	TPage ( string s = "" , int ns = NS_UNKNOWN ) { name = space2_(trim(_2space(s))) ; meta.ns = ns ; }
	
	const string getNameWithoutNamespace() ;
	void determineNamespace ( TPageList *pl ) ;
	
	string name ;
	TPageMetadata meta ;
} ;

inline bool operator < ( const TPage &t1 , const TPage &t2 ) { return (t1.name == t2.name ? t1.meta.ns < t2.meta.ns : t1.name < t2.name ) ; }
inline bool operator == ( const TPage &t1 , const TPage &t2 ) { return !((t1<t2)||(t2<t1)) ; }

#define PAGE_SORT_DEFAULT 0
#define PAGE_SORT_TITLE 1
#define PAGE_SORT_NS_TITLE 2
#define PAGE_SORT_SIZE 3
#define PAGE_SORT_DATE 4
#define PAGE_SORT_FILE_SIZE 5
#define PAGE_SORT_UPLOAD_DATE 6
#define PAGE_SORT_INCOMING_LINKS 7
#define PAGE_SORT_RANDOM 8
#define PAGE_SORT_REDLINKS_COUNT 9


class TPageList {
public:
	TPageList ( string w = "" ) { wiki = w ; }
	void clear () { pages.clear() ; }
	void intersect ( TPageList &pl ) ;
	void merge ( TPageList &pl ) ;
	void negate ( TPageList &pl ) ;
	inline int32_t size () { return pages.size() ; }
	string getNamespaceString ( const int16_t ns ) ;
	int16_t getNamespaceNumber ( const string &ns ) ;
	void convertToWiki ( string new_wiki ) ;
	void convertWikidataToWiki ( string new_wiki ) ;
	void swap ( TPageList &pl ) ;
	void customSort ( uint8_t mode , bool ascending ) ;
	virtual bool error ( string s ) { return false ; }
	uint32_t annotateWikidataItem ( TWikidataDB &db , string wiki , map <string,TPage *> &name2o ) ;
	void join ( string cmd , TPageList &pl ) ;
	void loadMissingMetadata ( string wikidata_language ) ;
	inline bool hasDataLoaded() { return data_loaded ; }
	void regexpFilter ( string regexp ) ;
	
	string wiki ;
	vector <TPage> pages ;
	map <int16_t,string> ns_canonical , ns_local ;

protected:
	void loadNamespaces () ;
	void convertToWikidata () ;
	bool is_sorted = false ;
	bool namespaces_loaded = false ;
	void sort() ;
	
	map <string,int16_t> ns_string2id ;
	bool data_loaded = false ;
} ;



class TSource : public TPageList {
public:
	TSource ( TPlatform *p = NULL ) { platform = p ; run_result = false ; }
	string getSourceName() { return source_name ; }
	virtual bool error ( string s ) ;
	virtual bool run () {} ;
	virtual bool getLastRunResult() { return run_result ; }

protected:
	TPlatform *platform ;
	string source_name ;
	bool run_result ;
} ;


class TSourceSPARQL : public TSource {
public:
	TSourceSPARQL ( TPlatform *p = NULL ) { platform = p ; source_name = "sparql" ; } ;
	virtual bool run () ;

protected:
	bool runQuery ( string query ) ;

	string sparql_prefixes = "PREFIX v: <http://www.wikidata.org/prop/statement/>\nPREFIX q: <http://www.wikidata.org/prop/qualifier/>\nPREFIX ps: <http://www.wikidata.org/prop/statement/>\nPREFIX pq: <http://www.wikidata.org/prop/qualifier/>\n" ;
} ;

class TSourceLabels : public TSource {
public:
	TSourceLabels ( TPlatform *p = NULL ) { platform = p ; source_name = "labels" ; } ;
	virtual bool run () ;
} ;

class TSourcePagePile : public TSource {
public:
	TSourcePagePile ( TPlatform *p = NULL ) { platform = p ; source_name = "pagepile" ; } ;
	virtual bool run () ;

protected:
	bool getPile ( uint32_t id ) ;
} ;

class TSourceSearch : public TSource {
public:
	TSourceSearch ( TPlatform *p = NULL ) { platform = p ; source_name = "search" ; } ;
	virtual bool run () ;

protected:
} ;

class TSourceManual : public TSource {
public:
	TSourceManual ( TPlatform *p = NULL ) { platform = p ; source_name = "manual" ; } ;
	virtual bool run () ;

protected:
	bool parseList ( string &text , string &new_wiki ) ;
} ;

class TSourceWikidata : public TSource {
public:
	TSourceWikidata ( TPlatform *p = NULL ) { platform = p ; source_name = "wikidata" ; } ;
	virtual bool run () ;

protected:
	bool getData ( string sites ) ;
} ;


class TSourceDatabaseCatDepth {
public:
	TSourceDatabaseCatDepth ( string n = "" , int16_t d = -1 ) { name = n ; depth = d ; }
	string name ;
	int16_t depth ;
} ;

class TSourceDatabaseParams {
public:
	vector <TSourceDatabaseCatDepth> positive , negative ;
	vector <uint16_t> page_namespace_ids ;
	vector <string> templates_yes , templates_any , templates_no ;
	bool templates_yes_talk_page = false ;
	bool templates_any_talk_page = false ;
	bool templates_no_talk_page = false ;
	vector <string> linked_from_all , linked_from_any , linked_from_none ;
	vector <string> links_to_all , links_to_any , links_to_none ;
	
	string wiki = "enwiki" ;
	int16_t default_depth = 0 ;
	string combine = "subset" ;
	string redirects = "either" ;
	string last_edit_bot = "either" ;
	string last_edit_anon = "either" ;
	string last_edit_flagged = "either" ;
	string page_image = "any" ;
	string ores_type = "any" ;
	string ores_prediction = "any" ;
	float ores_prob_from = 0 ;
	float ores_prob_to = 1.0 ;
	int32_t larger , smaller , minlinks , maxlinks ;
	string before , after , max_age , page_wikidata_item ;
	bool only_new_since = false ;
} ;

class TSourceDatabase : public TSource {
public:
	TSourceDatabase ( TPlatform *p = NULL ) { platform = p ; source_name = "categories" ; } ;
	static string listEscapedStrings ( TWikidataDB &db , vector <string> &s , bool fix_spaces = true ) ;
	virtual void setPageList ( TPageList *pagelist ) { primary_pagelist = pagelist ; }
	virtual bool run () ;

protected:
	bool getPages () ;
	bool parseCategoryList ( TWikidataDB &db , vector <TSourceDatabaseCatDepth> &input , vector <vector<string> > &output ) ;
	void getCategoriesInTree ( TWikidataDB &db , string name , int16_t depth , vector <string> &ret ) ;
	void goDepth ( TWikidataDB &db , map <string,bool> &tmp , vector <string> &cats , int16_t left ) ;
	string templateSubquery ( TWikidataDB &db , vector <string> input , bool use_talk_page , bool find_not ) ;
	string linksFromSubquery ( TWikidataDB &db , vector <string> input ) ;
	string linksToSubquery ( TWikidataDB &db , vector <string> input ) ;
	void groupLinkListByNamespace ( vector <string> &input , map <int32_t,vector <string> > &nslist ) ;

	TSourceDatabaseParams params ;
	TPageList *primary_pagelist = NULL ;
} ;


class TPlatform {
public:
	bool readConfigFile ( string filename ) ;
	void setConfig ( TPlatform &p ) ;
	bool error ( string s ) { errors.push_back ( s ) ; return false ; } ;
	string process() ;
	string getWiki () ;
	string getParam ( string key , string default_value = "" , bool ignore_empty = false ) ;
	float getQueryTime() { return querytime ; }
	string getExistingLabel ( string name ) ;
	bool doOutputRedlinks() { return output_redlinks ; }
	void setDatabaseParameters ( TSourceDatabaseParams &db_params ) ;
	string getLabelBaseSQL ( TWikidataDB &db ) ;
	
	map <string,string> config , params ;
	string content_type , query ;
	vector <string> errors ;
	uint32_t psid = 0 ;

protected:
	
	void parseCats ( string input , vector <TSourceDatabaseCatDepth> &output ) ;
	void splitParamIntoVector ( string input , vector <string> &output ) ;
	void processFiles ( TPageList &pl ) ;
	void annotateFile ( TWikidataDB &db , map <string,TPage *> &name2f , bool file_data , bool file_usage , bool file_usage_data_ns0 ) ;
	void processPages ( TPageList &pl ) ;
	void processSubpages ( TPageList &pl ) ;
	void annotatePage ( TWikidataDB &db , map <uint32_t,vector <TPage *> > &ns_pages , bool add_image , bool add_coordinates , bool add_defaultsort , bool add_disambiguation , bool add_incoming_links ) ;
	void processWikidata ( TPageList &pl ) ;
	void processSitelinks ( TPageList &pagelist ) ;
	void processLabels ( TPageList &pagelist ) ;
	void processCreator ( TPageList &pagelist ) ;
	void filterWikidata ( TPageList &pagelist ) ;
	void processMissingDatabaseFilters ( TPageList &pagelist ) ;
	void getCommonWikiAuto ( map <string,TSource *> &sources ) ;
	void combine ( TPageList &pagelist , map <string,TSource *> &sources ) ;
	void sortResults ( TPageList &pagelist ) ;
	void processRedlinks ( TPageList &pagelist ) ;
	void legacyAutoListParameters () ;
	void getParameterAsStringArray ( string s , vector <string> &vs ) ;
	string legacyCombinationParameters ( map <string,TSource *> &sources ) ;
	
	float querytime = 0 ; // seconds
	string wiki , common_wiki ;
	map <string,bool> existing_labels ;
	bool output_redlinks = false ;
	bool label_filter_used_as_primary = false ;
} ;

class TRenderer {
public:
	TRenderer ( TPlatform *p = NULL ) { platform = p ; }
	string renderPageList ( TPageList &pagelist ) ;
	bool only_files = false ;

private:
	string renderPageListHTML ( TPageList &pagelist ) ;
	string renderPageListJSON ( TPageList &pagelist ) ;
	string renderPageListWiki ( TPageList &pagelist ) ;
	string renderPageListCTSV ( TPageList &pagelist , string mode ) ;
	string renderPageListPagePile ( TPageList &pagelist ) ;
	string getLink ( TPage &page ) ;
	void initializeColumns() ;
	string getTableHeaderHTML() ;
	string getTableRowHTML ( uint32_t cnt , TPage &page , TPageList &pagelist ) ;
	string getTableRowCTSV ( uint32_t cnt , TPage &page , TPageList &pagelist , string &mode ) ;
	void escapeCSV ( string &out ) ;
	void addPageMetadataToJSON ( vector <TPage>::iterator i , json &o ) ;

	vector <string> columns ;
	TPlatform *platform = NULL ;
	string wiki ;
	bool use_autolist , autolist_creator_mode , is_wikidata ;
	string thumnail_size = "120px" ;
	struct timeval now_ish ;
} ;

#endif