#include "main.h"

string TPlatform::process () {

	TSourceDatabase db ( this ) ;
	TSourceDatabaseParams db_params ;
	db_params.wiki = getWiki() ;
	
	// Get valid namespaces
	for ( auto i = params.begin() ; i != params.end() ; i++ ) {
		string key = i->first ;
		if ( key.length() >= 5 && key[0]=='n' && key[1]=='s' && key[2]=='[' && key[key.length()-1]==']' ) {
			string nss = key.substr(3,key.length()-4) ;
			db_params.page_namespace_ids.push_back ( atoi ( nss.c_str() ) ) ;
		}
	}
	
	string dummy = getParam ( "categories" , "" ) ;
	vector <string> pos ;
	split ( dummy , pos , '\n' ) ;
	for ( auto i = pos.begin() ; i != pos.end() ; i++ ) {
		if ( i->empty() ) continue ;
		vector <string> parts ;
		split ( (*i) , parts , '|' , 2 ) ;
		if ( parts.empty() ) continue ; // Huh?
		if ( parts.size() < 2 ) parts.push_back ( getParam("depth","0") ) ;
		db_params.positive.push_back ( TSourceDatabaseCatDepth ( trim(parts[0]) , atoi(parts[1].c_str()) ) ) ;
	}
	
	TPageList pagelist ( getWiki() ) ;
	
	if ( db.getPages ( db_params ) ) {
		pagelist.swap ( db ) ;
	}
	
	// TODO: sort
	
	
	return renderPageList ( pagelist ) ;
}

string TPlatform::getWiki () {
	string l = getParam ( "language" , "en" ) ;
	string p = getParam ( "project" , "wikipedia" ) ;
	if ( l == "wikidata" || p == "wikidata" ) return "wikidatawiki" ;
	if ( l == "commons" || l == "meta" ) return l+"wiki" ;
	if ( p == "wikipedia" ) return l+"wiki" ;
	return l+p ;
}

string TPlatform::getParam ( string key , string default_value ) {
	auto i = params.find(key) ;
	if ( i != params.end() ) return i->second ;
	return default_value ;
}

string TPlatform::renderPageListHTML ( TPageList &pagelist ) {
	content_type = "text/html" ;

	string ret ;
	char tmp[100] ;
	sprintf ( tmp , "<hr/><h2><a name='results'></a>%ld results</h2>" , pagelist.pages.size() ) ;
	ret += tmp ;
	if ( pagelist.pages.size() == 0 ) return ret ; // No need for empty table
	ret += "<table class='table table-sm table-striped'>" ;
	ret += "<thead><tr><th class='num'>#</th><th class='text-nowrap l_h_title'></th><th class='text-nowrap l_h_id'></th><th class='text-nowrap l_h_namespace'></th><th class='text-nowrap l_h_len'></th><th class='text-nowrap l_h_touched'></th></tr></thead>" ;
	ret += "<tbody>" ;
	uint32_t cnt = 0 ;
	for ( auto i = pagelist.pages.begin() ; i != pagelist.pages.end() ; i++ ) {
		cnt++ ;
		int16_t ns = i->meta.ns ;
		string nsname = pagelist.getNamespaceString(ns) ;
		if ( nsname.empty() ) nsname = "<span class='l_namespace_0'>Article</span>" ;
		ret += "<tr>" ;
		sprintf ( tmp , "<td class='num'>%d</td>" , cnt ) ;
		ret += tmp ;
		ret += "<td>" + getLink ( *i ) + "</td>" ;
		sprintf ( tmp , "<td class='num'>%d</td>" , i->meta.id ) ; // ID
		ret += tmp ;
		ret += "<td>"+nsname+"</td>" ; // Namespace name
		sprintf ( tmp , "<td class='num'>%d</td>" , i->meta.size ) ; // Size
		ret += tmp ;
		ret += "<td class='num'>" + string(i->meta.timestamp) + "</td>" ; // TS
		ret += "</tr>" ;
	}
	ret += "</tbody>" ;
	ret += "</table>" ;
	return ret ;
}

string TPlatform::renderPageList ( TPageList &pagelist ) {
	string format = getParam ( "format" , "html" ) ;
	string ret ;
	
	content_type = "text/html" ; // Default
	
	if ( format == "dummy" ) {
	} else return renderPageListHTML ( pagelist ) ;
	
	return "" ;
}

string TPlatform::getLink ( const TPage &page ) {
	string label = page.name ;
	string url = page.name ;
	std::replace ( label.begin(), label.end(), '_', ' ') ;
	std::replace ( url.begin(), url.end(), ' ', '_') ;
	// TODO escape '
//	url = urlencode ( url ) ;
	url = "https://" + getWikiServer ( getWiki() ) + "/wiki/" + url ;
	return "<a target='_blank' href='" + url + "'>" + label + "</a>" ;
}

bool TPlatform::readConfigFile ( string filename ) {
	char *buffer = loadFileFromDisk ( filename ) ;
	MyJSON j ( buffer ) ;
	free (buffer);
	
	for ( auto i = j.o.begin() ; i != j.o.end() ; i++ ) {
		if ( i->first == "" ) continue ;
		config[i->first] = i->second.s ;
	}

	return true ;
}
