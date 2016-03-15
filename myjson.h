#ifndef __MYJSON_H__
#define __MYJSON_H__

// THIS SHOULD PROBABLY BE REPLACED BY
// https://github.com/nlohmann/json

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>

using namespace std ;

#define MJ_UNKNOWN 0
#define MJ_STRING 1 // String
#define MJ_INT 2 // Integer
#define MJ_FLOAT 3 // Float
#define MJ_ARRAY 4 // Array
#define MJ_OBJECT 5 // Object
#define MJ_NULL 6 // Null

class MyJSON ;

// TODO size checks etc.
class MyJSON {
public :
	MyJSON ( char *t = NULL ) { isa = MJ_UNKNOWN ; i = 0 ; f = 0 ; if ( t ) parse ( t ) ; }
	inline bool has(string s) { return o.find(s)!=o.end() ; }
	inline uint32_t size() { return isa==MJ_ARRAY?a.size():o.size() ; }
	inline MyJSON &get ( uint32_t key ) { return a[key] ; }
	inline MyJSON &get ( string key ) { return o[key] ; }
	void print ( ostream &out ) ;
	
	inline MyJSON & operator [] ( uint32_t key ) { return a[key] ; } // Arrays
	inline MyJSON & operator [] ( string key ) { return o[key] ; } // Objects

	string escapeString ( const string &s ) ;
	string unescapeString ( const string &input ) ;
	
	// TODO operators to cast into string, number

	uint8_t isa ; // Is a type MJ_
	string s ;
	vector <MyJSON> a ;
	map <string,MyJSON> o ;
	float f ;
	int32_t i ;

protected :
	char *parse ( char *t , int depth = 0 ) ;
	static string to_unicode(const std::size_t codepoint1, const std::size_t codepoint2 = 0) ;
} ;


#endif
