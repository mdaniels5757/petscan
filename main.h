#ifndef __main_h__
#define __main_h__

#include <mysql.h>
#include <curl/curl.h>

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

#include "myjson.h"
#include <stdio.h>
#include <string.h>

using namespace std ;


#define NS_UNKNOWN -999

char *loadFileFromDisk ( string filename ) ;

class TPlatform ;

class TWikidataDB {
public:
	TWikidataDB () {} ;
	TWikidataDB ( TPlatform &platform , string wiki ) ;
//	bool updateRecentChanges ( TItemSet &target ) ;
//	void getRedirects ( map <TItemNumber,bool> &remove ) ;
//	void getDeletedItems ( map <TItemNumber,bool> &remove ) ;
	void doConnect ( bool first = false ) ;
	void runQuery ( string sql ) ;
	MYSQL_RES *getQueryResults ( string sql ) ;
	string escape ( string s ) ;
	string space2_ ( string s ) ;
	~TWikidataDB () ;

	uint32_t batch_size ; // ATTENTION: If this is lower than the number of edits in a specific second, it may cause a loop. Default is 1000; keep it well >100 !
	
protected:
	MYSQL mysql;
	string _host , _config_file , _database ;
	TPlatform *_platform ;
	
	char *getTextFromURL ( string url ) ;
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




class TMetadata {
public:
} ;

class TPage {
public:
	TPage ( const char *s = NULL ) { if(s) name = s ; }
	TPage ( string s ) { name = s ; }
	TPage ( string s , int _ns) { name = s ; ns = _ns ; }
	string name ;
	int16_t ns = NS_UNKNOWN ;
	TMetadata *meta = NULL ;
} ;

inline bool operator < ( const TPage &t1 , const TPage &t2 ) { return t1.name < t2.name ; }
inline bool operator == ( const TPage &t1 , const TPage &t2 ) { return !((t1<t2)||(t2<t1)) ; }


class TPageList {
public:
	void clear () { pages.clear() ; }
	void intersect ( TPageList &pl ) ;
	void merge ( TPageList &pl ) ;
	inline int32_t size () { return pages.size() ; }
	
	string wiki ;
	vector <TPage> pages ;
	virtual bool error ( string s ) { return false ; }
protected:
	bool is_sorted = false ;
	void sort() ;
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
	vector <string> templates_all , templates_any , templates_none ;
	bool templates_all_talk_page = false ;
	bool templates_any_talk_page = false ;
	bool templates_none_talk_page = false ;
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

protected:
	void getCategoriesInTree ( TWikidataDB &db , string name , int16_t depth , vector <string> &ret ) ;
	void goDepth ( TWikidataDB &db , map <string,bool> &tmp , vector <string> &cats , int16_t left ) ;
	string listEscapedStrings ( TWikidataDB &db , vector <string> &s ) ;
} ;


class TPlatform {
public:
	bool readConfigFile ( string filename ) ;
	bool error ( string s ) { cout << s << endl ; return false ; } ;
	map <string,string> config ;
} ;

#endif