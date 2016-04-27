#!/usr/bin/php
<?PHP

require_once ( './shared.php' ) ;

$petscan = 'https://petscan.wmflabs.org' ;

function getFromPSID ( $psid ) {
	global $petscan ;
	$url = "$petscan/?psid=$psid&format=json&sparse=1" ;
	$j = json_decode ( file_get_contents ( $url ) ) ;
	$star = '*' ;
	$ret = $j->$star ;
	$ret = $ret[0]->a->$star ;
	return $ret ;
}

function getPagePile ( $pile ) {
	$url = "https://tools.wmflabs.org/pagepile/api.php?id=$pile&action=get_data&format=json&doit" ;
	$j = json_decode ( file_get_contents ( $url ) ) ;
	return $j->pages ;
}

function getSubset ( $a1 , $a2 ) {
	$ret = array() ;
	foreach ( $a1 AS $a ) {
		if ( in_array ( $a , $a2 ) ) $ret[] = $a ;
	}
	return $ret ;
}

function compareArrays ( $name , $a1 , $a2 ) {
	$only1 = array() ;
	$only2 = array() ;
	$both = 0 ;
	
	foreach ( $a1 AS $a ) {
		if ( !in_array ( $a , $a2 ) ) {
			$only1[] = $a ;
		} else $both++ ;
	}

	foreach ( $a2 AS $a ) {
		if ( !in_array ( $a , $a1 ) ) {
			$only2[] = $a ;
		}
	}
	
	if ( count($only1) == 0 and count($only2) == 0 and count($a1) == $both ) {
		print "$name\tPASSED\n" ;
		return ;
	}
	
	print "$name\tFAILED\n" ;
	foreach ( $only1 AS $a ) print "* Only in list 1: $a\n" ;
	foreach ( $only2 AS $a ) print "* Only in list 2: $a\n" ;
}



// TESTS

function testCategories () {
	$db = openDBwiki ( 'dewiki' ) ;
	$mann = getPagesInCategory ( $db , 'Mann' , 2 , 0 , true ) ;
	$frau = getPagesInCategory ( $db , 'Frau' , 2 , 0 , true ) ;
	$subset = getSubset ( $mann , $frau ) ;
	$psd = getFromPSID ( 18390 ) ;
	compareArrays ( "Categories" , $subset , $psd ) ;
}

function testPagePile () {
	$db = openDBwiki ( 'enwiki' ) ;
	$cat = getPagesInCategory ( $db , '1995 Formula One races' , 1 , 0 , true ) ;
	$pages = getPagePile ( 2939 ) ;
	$subset = getSubset ( $cat , $pages ) ;
	$psd = getFromPSID ( 18588 ) ;
	compareArrays ( "PagePile" , $subset , $psd ) ;
}

function testSPARQL () {
	$sparql = 'SELECT ?item WHERE { ?item wdt:P180 wd:Q148706 }' ;
	$items_sparql = getSPARQLitems ( $sparql , 'item' ) ;
	$items_pagepile = getPagePile ( 2941 ) ;
	$subset = getSubset ( $items_sparql , $items_pagepile ) ;
	$psd = getFromPSID ( 18607 ) ;
	compareArrays ( "SPARQL" , $subset , $psd ) ;
}


// TEST CALLS
testPagePile() ;
testSPARQL() ;
testCategories() ;

?>