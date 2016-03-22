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

#include "json.hpp"
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <sys/time.h>

using namespace std ;
using json = nlohmann::json;


#define NS_UNKNOWN -999
#define UNKNOWN_WIKIDATA_ITEM 0
#define DB_PAGE_BATCH_SIZE 50000
#define NS_FILE 6
#define MAX_QUERY_OUTPUT_LENGTH 2000

static vector <string> file_data_keys = { "img_size","img_width","img_height","img_media_type","img_major_mime","img_minor_mime","img_user_text","img_timestamp","img_sha1" } ;

char *loadFileFromDisk ( string filename ) ;
void split ( const string &input , vector <string> &v , char delim , uint32_t max = 0 ) ;
const std::string urlencode( const std::string& s ) ;
const std::string urldecode ( const std::string& str ) ;
string getWikiServer ( string wiki ) ;
bool loadJSONfromURL ( string url , json &j ) ;
string space2_ ( string s ) ;
string _2space ( string s ) ;
string ui2s ( uint32_t i ) ;
string escapeURLcomponent ( string s ) ;
double time_diff(struct timeval x , struct timeval y);

class TPlatform ;

class TWikidataDB {
public:
	TWikidataDB () {} ;
	TWikidataDB ( TPlatform &platform , string wiki ) ;
	void doConnect ( bool first = false ) ;
	void runQuery ( string sql ) ;
	MYSQL_RES *getQueryResults ( string sql ) ;
	string escape ( string s ) ;
	string space2_ ( string s ) ;
	~TWikidataDB () ;
	
protected:
	MYSQL mysql;
	string _host , _config_file , _database ;
	TPlatform *_platform ;
	
//	char *getTextFromURL ( string url ) ;
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
	TPage ( string s ) { name = s ; }
	TPage ( string s , int ns = 0 ) { name = space2_(s) ; meta.ns = ns ; }
	
	const string getNameWithoutNamespace() ;
	
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


class TPageList {
public:
	TPageList ( string w = "" ) { wiki = w ; }
	void clear () { pages.clear() ; }
	void intersect ( TPageList &pl ) ;
	void merge ( TPageList &pl ) ;
	inline int32_t size () { return pages.size() ; }
	string getNamespaceString ( const int16_t ns ) ;
	int16_t getNamespaceNumber ( const string &ns ) ;
	inline void swap ( TPageList &pl ) {
		wiki.swap ( pl.wiki ) ;
		pages.swap ( pl.pages ) ;
	}
	void customSort ( uint8_t mode , bool ascending ) ;
	virtual bool error ( string s ) { return false ; }
	
	string wiki ;
	vector <TPage> pages ;
	map <int16_t,string> ns_canonical , ns_local ;
protected:
	void loadNamespaces () ;
	bool is_sorted = false ;
	bool namespaces_loaded = false ;
	void sort() ;
	map <string,int16_t> ns_string2id ;
} ;



class TSource : public TPageList {
public:
	TSource ( TPlatform *p = NULL ) { platform = p ; } ;
	TPlatform *platform ;
	
	virtual bool error ( string s ) ;
} ;


class TSourceSPARQL : public TSource {
public:
	TSourceSPARQL ( TPlatform *p = NULL ) { platform = p ; } ;
	bool runQuery ( string query ) ;
protected:
	string sparql_prefixes = "PREFIX v: <http://www.wikidata.org/prop/statement/>\nPREFIX q: <http://www.wikidata.org/prop/qualifier/>\n" ;


} ;

class TSourcePagePile : public TSource {
public:
	TSourcePagePile ( TPlatform *p = NULL ) { platform = p ; } ;
	
	bool getPile ( uint32_t id ) ;
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
	
	string wiki = "enwiki" ;
	int16_t default_depth = 0 ;
	string combine = "subset" ;
	string redirects = "either" ;
	string last_edit_bot = "either" ;
	string last_edit_anon = "either" ;
	string last_edit_flagged = "either" ;
} ;

class TSourceDatabase : public TSource {
public:
	TSourceDatabase ( TPlatform *p = NULL ) { platform = p ; } ;
	
	bool getPages ( TSourceDatabaseParams &params ) ;
	static string listEscapedStrings ( TWikidataDB &db , vector <string> &s , bool fix_spaces = true ) ;

protected:
	bool parseCategoryList ( TWikidataDB &db , vector <TSourceDatabaseCatDepth> &input , vector <vector<string> > &output ) ;
	void getCategoriesInTree ( TWikidataDB &db , string name , int16_t depth , vector <string> &ret ) ;
	void goDepth ( TWikidataDB &db , map <string,bool> &tmp , vector <string> &cats , int16_t left ) ;
	string templateSubquery ( TWikidataDB &db , vector <string> input , bool use_talk_page , bool find_not ) ;
	string linksFromSubquery ( TWikidataDB &db , vector <string> input ) ;
} ;


class TPlatform {
public:
	bool readConfigFile ( string filename ) ;
	bool error ( string s ) { cout << s << endl ; return false ; } ;
	string process() ;
	string getWiki () ;
	string getParam ( string key , string default_value = "" ) ;
	
	map <string,string> config , params ;
	string content_type , query ;

protected:
	string renderPageList ( TPageList &pagelist ) ;
	string renderPageListHTML ( TPageList &pagelist ) ;
	string renderPageListJSON ( TPageList &pagelist ) ;
	string renderPageListWiki ( TPageList &pagelist ) ;
	string renderPageListTSV ( TPageList &pagelist ) ;
	
	string getLink ( TPage &page ) ;
	void parseCats ( string input , vector <TSourceDatabaseCatDepth> &output ) ;
	void splitParamIntoVector ( string input , vector <string> &output ) ;
	void processFiles ( TPageList &pl ) ;
	void annotateFile ( TWikidataDB &db , map <string,TPage *> &name2f , bool file_data , bool file_usage ) ;
	void processWikidata ( TPageList &pl ) ;
	uint32_t annotateWikidataItem ( TWikidataDB &db , string wiki , map <string,TPage *> &name2o ) ;
	float querytime = 0 ; // seconds
} ;




// trim from start
static inline std::string &ltrim(std::string &s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
        return s;
}

// trim from end
static inline std::string &rtrim(std::string &s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
        return s;
}

// trim from both ends
static inline std::string &trim(std::string &s) {
        return ltrim(rtrim(s));
}

#endif