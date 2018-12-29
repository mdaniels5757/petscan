#include "main.h"

#define ITEM_BATCH_SIZE 10000

typedef pair <string,string> t_title_q ;

void TWDFIST::filterItems () {
	// Chunk item list
	vector <string> item_batches ;
	for ( uint64_t cnt = 0 ; cnt < items.size() ; cnt++ ) {
		if ( cnt % ITEM_BATCH_SIZE == 0 ) item_batches.push_back ( "" ) ;
		if ( !item_batches[item_batches.size()-1].empty() ) item_batches[item_batches.size()-1] += "," ;
		item_batches[item_batches.size()-1] += "\"" + items[cnt] + "\"" ;
	}

	// Check items
	vector <string> new_items ;
	new_items.reserve ( items.size() ) ;
	TWikidataDB wd_db ( "wikidatawiki" , platform );
	for ( size_t chunk = 0 ; chunk < item_batches.size() ; chunk++ ) {
		string sql = "SELECT page_title FROM page WHERE page_namespace=0 AND page_is_redirect=0" ;
		sql += " AND page_title IN (" + item_batches[chunk] + ")" ;
		if ( wdf_only_items_without_p18 ) sql += " AND NOT EXISTS (SELECT * FROM pagelinks WHERE pl_from=page_id AND pl_namespace=120 AND pl_title='P18')" ;
		MYSQL_RES *result = wd_db.getQueryResults ( sql ) ;
		MYSQL_ROW row;
		while ((row = mysql_fetch_row(result))) {
			new_items.push_back ( row[0] ) ;
		}
		mysql_free_result(result);
	}
	items.swap ( new_items ) ;
}

string TWDFIST::normalizeFilename ( string filename ) {
	string ret = trim ( filename ) ;
	replace ( ret.begin(), ret.end(), ' ', '_' ) ;
	json o = ret ;
	try { // HACK
		string dummy = o.dump() ;
	} catch ( ... ) {
		ret = "" ;
	}
	return ret ;
}

bool TWDFIST::isValidFile ( string file ) { // Requires normalized filename
	if ( file.empty() ) return false ;

	// Check files2ignore
	if ( files2ignore.find(file) != files2ignore.end() ) return false ;

	// Check type
	size_t dot = file.find_last_of ( "." ) ;
	string type = file.substr ( dot+1 ) ;
	std::transform(type.begin(), type.end(), type.begin(), ::tolower);
	if ( wdf_only_jpeg && type!="jpg" && type!="jpeg" ) return false ;
	if ( type == "svg" && !wdf_allow_svg ) return false ;
	if ( type == "pdf" || type == "gif" ) return false ;

	// Check key phrases
	if ( file.find("Flag_of_") == 0 ) return false ;
	if ( file.find("Crystal_Clear_") == 0 ) return false ;
	if ( file.find("Nuvola_") == 0 ) return false ;
	if ( file.find("Kit_") == 0 ) return false ;

// 	if ( preg_match ( '/\bribbon.jpe{0,1}g/i' , $i ) ) // TODO
//	if ( preg_match ( '/^600px_.+\.png/i' , $i ) ) // TODO
	
	return true ;
}

void TWDFIST::seedIgnoreFiles () {
	// Load wiki list
	string wikitext = loadTextfromURL ( "http://www.wikidata.org/w/index.php?title=User:Magnus_Manske/FIST_icons&action=raw" ) ;
	vector <string> rows ;
	split ( wikitext , rows , '\n' ) ;
	for ( size_t row = 0 ; row < rows.size() ; row++ ) {
		string file = rows[row] ;
		while ( !file.empty() && (file[0]=='*'||file[0]==' ') ) file.erase ( file.begin() , file.begin()+1 ) ;
		file = normalizeFilename ( trim ( file ) ) ;
		if ( !isValidFile(file) ) continue ;
		files2ignore[file] = 1 ;
	}

	// Load files that were ignored at least three times
	TWikidataDB wdfist_db ;
	wdfist_db.setHostDB ( "tools.labsdb" , "s51218__wdfist_p" ) ; // HARDCODED publicly readable
	wdfist_db.doConnect ( true ) ;
	string sql = "SELECT CONVERT(`file` USING utf8) FROM ignore_files GROUP BY file HAVING count(*)>=3" ;
	MYSQL_RES *result = wdfist_db.getQueryResults ( sql ) ;
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) {
		string file = normalizeFilename ( row[0] ) ;
		if ( !isValidFile(file) ) continue ;
		files2ignore[file] = 1 ;
	}
	mysql_free_result(result);
}

void TWDFIST::followLanguageLinks () {
	// Chunk item list
	vector <string> item_batches ;
	for ( uint64_t cnt = 0 ; cnt < items.size() ; cnt++ ) {
		if ( cnt % ITEM_BATCH_SIZE == 0 ) item_batches.push_back ( "" ) ;
		if ( !item_batches[item_batches.size()-1].empty() ) item_batches[item_batches.size()-1] += "," ;
		item_batches[item_batches.size()-1] += "\"" + items[cnt] + "\"" ;
	}

	// Get sitelinks
	map <string,vector <t_title_q> > titles_by_wiki ;
	TWikidataDB wd_db ( "wikidatawiki" , platform );
	for ( uint64_t x = 0 ; x < item_batches.size() ; x++ ) {
		string item_ids ;
		item_ids.reserve ( item_batches[x].size() ) ;
		for ( uint64_t p = 0 ; p < item_batches[x].size() ; p++ ) {
			if ( item_batches[x][p] != 'Q' ) item_ids += item_batches[x][p] ;
		}
		string sql = "SELECT ips_item_id,ips_site_id,ips_site_page FROM wb_items_per_site WHERE ips_item_id IN (" + item_ids + ")" ;
		MYSQL_RES *result = wd_db.getQueryResults ( sql ) ;
		MYSQL_ROW row;
		while ((row = mysql_fetch_row(result))) {
			string wiki = row[1] ;
			if ( wiki == "wikidatawiki" ) continue ; // Not relevant
			string q = "Q" + string(row[0]) ;
			string title = row[2] ;
			replace ( title.begin(), title.end(), ' ', '_' ) ;
			titles_by_wiki[wiki].push_back ( t_title_q ( title , q ) ) ;
		}
		mysql_free_result(result);
	}

	// Get images for sitelinks from globalimagelinks
	TWikidataDB commons_db ( "commonswiki" , platform );
	for ( auto wp = titles_by_wiki.begin() ; wp != titles_by_wiki.end() ; wp++ ) {
		string wiki = wp->first ;
		vector <string> parts ;
		map <string,string> title2q ;
		parts.push_back ( "" ) ;
		for ( auto tq = wp->second.begin() ; tq != wp->second.end() ; tq++ ) {
			if ( parts[parts.size()-1].size() > 500000 ) parts.push_back ( "" ) ; // String length limit
			if ( !parts[parts.size()-1].empty() ) parts[parts.size()-1] += "," ;
			parts[parts.size()-1] += "\"" + commons_db.escape(tq->first) + "\"" ;
			title2q[tq->first] = tq->second ;
		}

		for ( auto part = parts.begin() ; part != parts.end() ; part++ ) {
			if ( part->empty() ) continue ;

			// Page images
			map <string,string> title2file ;
			if ( wdf_only_page_images ) {
				TWikidataDB local_wiki ( wiki , platform ) ;
				string sql = "SELECT page_title,pp_value FROM page,page_props WHERE page_id=pp_page AND page_namespace=0 AND pp_propname='page_image_free' AND page_title IN (" + *part + ")" ;
				MYSQL_RES *result = local_wiki.getQueryResults ( sql ) ;
				MYSQL_ROW row;
				while ((row = mysql_fetch_row(result))) {
					string title = row[0] ;
					string file = normalizeFilename ( row[1] ) ;
					if ( !isValidFile ( file ) ) continue ;
					title2file[title] = file ;
				}
				mysql_free_result(result);
			}

			if ( 1 ) {
				string sql = "SELECT DISTINCT gil_page_title AS page,gil_to AS image FROM page,globalimagelinks WHERE gil_wiki='" + wiki + "' AND gil_page_namespace_id=0" ;
				sql += " AND gil_page_title IN (" + *part + ") AND page_namespace=6 and page_title=gil_to AND page_is_redirect=0" ;
				sql += " AND NOT EXISTS (SELECT * FROM categorylinks where page_id=cl_from and cl_to='Crop_for_Wikidata')" ; // To-be-cropped
				MYSQL_RES *result = commons_db.getQueryResults ( sql ) ;
				MYSQL_ROW row;
				while ((row = mysql_fetch_row(result))) {
					string title = row[0] ;
					string file = normalizeFilename ( row[1] ) ;
					if ( wdf_only_page_images ) {
						if ( title2file.find(title) == title2file.end() ) continue ; // Page has no page image
						if ( title2file[title] != file ) continue ;
					}
					if ( !isValidFile ( file ) ) continue ;
					if ( title2q.find(title) == title2q.end() ) {
						cout << "Not found : " << title << endl ;
						continue ;
					}
					string q = title2q[title] ;
					if ( q2image.find(q) == q2image.end() ) q2image[q] = string2int32 () ;
					if ( q2image[q].find(file) == q2image[q].end() ) q2image[q][file] = 1 ;
					else q2image[q][file]++ ;
				}
				mysql_free_result(result);
			}
		}

	}

}

void TWDFIST::followCoordinates () {} // TODO
void TWDFIST::followSearchCommons () {} // TODO
void TWDFIST::followCommonsCats () {} // TODO

string TWDFIST::run () {
	// Init JSON output
	json j ;
	j["status"] = "OK" ;
	j["data"] = json::object() ;

	platform->content_type = "application/json; charset=utf-8" ; // Output is always JSON
	pagelist->convertToWiki ( "wikidatawiki" ) ; // Making sure
	if ( pagelist->size() == 0 ) { // No items
		j["status"] = "No items from original query" ;
		return j.dump() ;
	}

	// Convert pagelist into item list, then save space by clearing pagelist
	items.reserve ( pagelist->size() ) ;
	for ( auto i = pagelist->pages.begin() ; i != pagelist->pages.end() ; i++ ) {
		if ( i->meta.ns != 0 ) continue ;
		items.push_back ( i->getNameWithoutNamespace() ) ;
	}
	pagelist->clear() ;

	// Follow
	wdf_langlinks = !(platform->getParam("wdf_langlinks","").empty()) ;
	wdf_coords = !(platform->getParam("wdf_coords","").empty()) ;
	wdf_search_commons = !(platform->getParam("wdf_search_commons","").empty()) ;
	wdf_commons_cats = !(platform->getParam("wdf_commons_cats","").empty()) ;

	// Options
	wdf_only_items_without_p18 = !(platform->getParam("wdf_only_items_without_p18","").empty()) ; // DONE
	wdf_only_files_not_on_wd = !(platform->getParam("wdf_only_files_not_on_wd","").empty()) ;
	wdf_only_jpeg = !(platform->getParam("wdf_only_jpeg","").empty()) ; // DONE
	wdf_max_five_results = !(platform->getParam("wdf_max_five_results","").empty()) ;
	wdf_only_page_images = !(platform->getParam("wdf_only_page_images","").empty()) ; // DONE
	wdf_allow_svg = !(platform->getParam("wdf_allow_svg","").empty()) ; // DONE

	// Prepare
	seedIgnoreFiles() ;
	filterItems() ;
	if ( items.size() == 0 ) {
		j["status"] = "No items from original query" ;
		return j.dump() ;
	}

	// Run followers
	if ( wdf_langlinks ) followLanguageLinks() ;
	if ( wdf_coords ) followCoordinates() ;
	if ( wdf_search_commons ) followSearchCommons() ;
	if ( wdf_commons_cats ) followCommonsCats() ;

	j["data"] = q2image ;
	return j.dump() ;
}