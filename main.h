#ifndef __main_h__
#define __main_h__

#include <mysql.h>
#include <curl/curl.h>

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

using namespace std ;

#include "myjson.h"


#define NS_UNKNOWN -999


class TPlatform ;

class TWikidataDB {
public:
	TWikidataDB () {} ;
	TWikidataDB ( string config_file , string host ) ;
//	bool updateRecentChanges ( TItemSet &target ) ;
//	void getRedirects ( map <TItemNumber,bool> &remove ) ;
//	void getDeletedItems ( map <TItemNumber,bool> &remove ) ;
	~TWikidataDB () ;

	uint32_t batch_size ; // ATTENTION: If this is lower than the number of edits in a specific second, it may cause a loop. Default is 1000; keep it well >100 !
	
protected:
	MYSQL mysql;
	string _host , _config_file , _database ;
	
	void doConnect ( bool first = false ) ;
	void runQuery ( string sql ) ;
	MYSQL_RES *getQueryResults ( string sql ) ;
	char *getTextFromURL ( string url ) ;
	void finishWithError ( string msg = "" ) ;


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
//	TPageList pagelist ;
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




class TPlatform {
public:
	void error ( string s ) { cout << s << endl ; } ;
} ;

#endif