var params = {} ;
var default_params = {
	'show_redirects':'both',
	'edits[bots]':'both',
	'edits[anons]':'both',
	'edits[flagged]':'both',
	'combination':'subset',
	'wpiu':'any',
	'format':'html',
	'sortby':'none',
	'depth':'0',
	'min_redlink_count':'1',
	'wikidata_item':'no',
	'output_compatability':'catscan',
	'active_tab':'tab_categories',
	'common_wiki':'auto',
	'sortorder':'ascending'
} ;

var thumbnail_size = '300px' ;
var max_namespace_id = 0 ;
var namespaces_selected = [] ;
var namespaces_loading = false ;
var last_namespaces = {} ;
var last_namespace_project = '' ;
var interface_language = '' ;
var interface_text = {} ;

var load_thumbnails_ahead = 2 ; // 1 to not load ahead
$.fn.is_on_screen = function(){
    var win = $(window);
    var viewport = {
        top : win.scrollTop(),
        left : win.scrollLeft()
    };
    viewport.right = viewport.left + win.width();
    viewport.bottom = viewport.top + win.height();
 
    var bounds = this.offset();
    bounds.right = bounds.left + this.outerWidth();
    bounds.bottom = bounds.top + this.outerHeight()*load_thumbnails_ahead;
 
    return (!(viewport.right < bounds.left || viewport.left > bounds.right || viewport.bottom < bounds.top || viewport.top > bounds.bottom));
};


function deXSS ( s ) {
	return s.replace ( /<\s*script/ , '' ) ; // TODO this should be better...
}

function getUrlVars () {
	var vars = {} ;
	var params = decodeURIComponent ( $('#querystring').text() ) ;
	if ( params == '' ) params = window.location.href.slice(window.location.href.indexOf('?') + 1) ;
	var hashes = params.split('&');
	if ( hashes.length >0 && hashes[0] == window.location.href ) hashes.shift() ;
	$.each ( hashes , function ( i , j ) {
		var hash = j.split('=');
		hash[1] += '' ;
		vars[decodeURIComponent(hash[0])] = decodeURIComponent(hash[1]).replace(/_/g,' ');
	} ) ;
	return vars;
}

function _t ( k , alt_lang ) {
	if ( typeof alt_lang == 'undefined' ) alt_lang = '' ;
	var l = interface_language ;
	if ( alt_lang != '' ) l = alt_lang ;
	var ret = "<i>" + k + "</i>" ;
	if ( typeof interface_text['en'][k] != 'undefined' ) ret = interface_text['en'][k] ;
	if ( typeof interface_text[l] != 'undefined' && typeof interface_text[l][k] != 'undefined' ) ret = interface_text[l][k] ;
	return ret ;
}

function setPermalink ( q ) {
	var psid = 0 ;
	$('span[name="psid"]').each ( function () { psid = $(this).text() ; } ) ;

	q = q.replace ( /&{0,1}doit=[^&]*&{0,1}/ , '&' ) ; // Removing auto-run
	
	// Removing empty parameters
	var lq ;
	do {
		lq = q ;
		q = q.replace ( /&[a-z_]+=&/g , '&' ) ;
	} while ( lq != q ) ;
	
	// Removing default values
	$.each ( default_params , function ( k , v ) {
		var key = '&{0,1}' + encodeURIComponent(k) + '=' + encodeURIComponent(v) + '&{0,1}' ;
		var r = new RegExp ( key ) ;
		q = q.replace ( r , '&' ) ;
	} ) ;
	
	var url = '/?' + q ;
	var h = _t("query_url") ;
	h = h.replace ( /\$1/ , url+"&doit=" ) ;
	h = h.replace ( /\$2/ , url ) ;
	
	if ( psid != 0 ) {
		var psid_note = (_t("psid_note")).split('$1').join(psid) ;// ( /\$1/ , psid ) ;
		h += ' ' ;
		h += psid_note ;
		h += " <span><a target='_blank' href='https://tools.wmflabs.org/fist/wdfist/index.html?psid="+psid+"&no_images_only=1&remove_used=1&remove_multiple=1&prefilled=1'>" ;
		h += _t("psid_image_link") ;
		h += "</a>.</span>" ;
	}
	
	$('#permalink').html ( h ) ;
}

function applyParameters () {
	
	namespaces_selected = [] ;
	
	$.each ( params , function ( name , value ) {
		
		var m = name.match ( /^ns\[(\d+)\]$/ ) ;
		if ( m != null ) {
			namespaces_selected.push ( m[1]*1 ) ;
			return ;
		}
		
		$('input:radio[name="'+name+'"][value="'+value.replace(/"/g,'&quot;')+'"]').prop('checked', true);
		
		$('input[type="text"][name="'+name+'"]').val ( deXSS(value) ) ;
		$('input[type="number"][name="'+name+'"]').val ( parseInt(value) ) ;
		$('textarea[name="'+name+'"]').val ( deXSS(value.replace(/\+/g,' ')) ) ;
		
		if ( value == '1' || value == 'on' ) $('input[type="checkbox"][name="'+name+'"]').prop('checked', true);
		
	} ) ;
	
	function wait2load_ns () {
		if ( namespaces_loading ) {
			setTimeout ( wait2load_ns , 100 ) ;
			return ;
		}
		
		loadNamespaces() ;
	}
	wait2load_ns() ;
	
	var q = decodeURIComponent ( $('#querystring').text() ) ;
	if ( q != '' ) setPermalink ( q ) ;

	if ( typeof params.active_tab != 'undefined' ) {
		var tab = '#' + params.active_tab.replace(/ /g,'_') ; //$('input[name="active_tab"]').val() ;
		$('#main_form ul.nav-tabs a[href="'+tab+'"]').click() ;
	}
	
	$('body').show() ;
}

function setInterfaceLanguage ( l ) {
	if ( interface_language == l ) return false ;
	interface_language = l ;
	$('input[name="interface_language"]').val ( l ) ;
	$.each ( interface_text['en'] , function ( k , v ) {
		if ( typeof interface_text[l][k] != 'undefined' ) v = interface_text[l][k] ;
		$('.l_'+k).html ( v ) ;
		$('.ph_'+k).attr ( { placeholder:v } ) ;
	} ) ;
	$('a.l_manual').attr ( { href:'https://meta.wikimedia.org/wiki/PetScan/'+l } ) ;
	$('a.l_interface_text').attr ( { href:'https://meta.wikimedia.org/wiki/PetScan/Interface#'+l.toUpperCase() } ) ;
//	if ( typeof autolist != 'undefined' ) autolist.setInterfaceLanguage ( l ) ;
	$('#doit').attr ( { value : _t('doit') } ) ;
	
	// Misc special updates
	$('#query_length').text ( _t('query_length').replace('$1',$('#query_length').attr('sec')) ) ;
	$('#num_results').text ( _t('num_results').replace('$1',$('#num_results').attr('num')) ) ;

	// Permalink
	var query = decodeURIComponent ( $('#querystring').text() ) ;
	if ( query != '' ) setPermalink ( query ) ;
	
	$('#permalink a').each ( function () {
		var a = $(this) ;
		var h = a.attr('href') ;
		var h2 = h.replace ( /\binterface_language=[^&]+/ , 'interface_language='+l ) ;
		if ( h == h2 ) h2 = h + "&interface_language=" + l ;
		a.attr ( { href : h2 } ) ;
	} ) ;
	
	return false ;
}

function loadInterface ( init_lang , callback ) {

	if ( typeof raw_interface_text_object == 'undefined' ) {
		setTimeout ( function () { loadInterface ( init_lang , callback ) } , 100 ) ;
		return ;
	}

	loadNamespaces ( function () {

		var wt = raw_interface_text_object.parse.wikitext['*'] ;
		var rows = wt.split("\n") ;
		var lang = '' ;
		$.each ( rows , function ( k , v ) {
			var m = v.match ( /^==\s*(.+?)\s*==$/ ) ;
			if ( m != null ) {
				lang = m[1].toLowerCase() ;
				interface_text[lang] = {} ;
			
				var h = '<a class="dropdown-item" href="#">' + m[1] + '</a>' ;
				$('#interface_languages').append ( h ) ;
				return ;
			}
			m = v.match ( /^;(.+?):(.*)$/ ) ;
			if ( m == null ) return ;
			if ( m[1] == 'toolname' ) m[2] = 'PetScan' ; // HARDHACK FIXME
			interface_text[lang][m[1]] = m[2] ;
		} ) ;
	
		$('#interface_languages a.dropdown-item').click ( function (e) {
			e.preventDefault();
			var o = $(this) ;
			var lang = o.text().toLowerCase() ;
			setInterfaceLanguage ( lang ) ;
		} ) ;
	
		setInterfaceLanguage ( init_lang ) ;

		$('input[name="language"]').keyup ( loadNamespaces ) ;
		$('input[name="project"]').keyup ( loadNamespaces ) ;


		applyParameters() ;
		if ( typeof callback != 'undefined' ) callback() ;
	
	} ) ;
}

function loadNamespaces ( callback ) {
	if ( namespaces_loading ) return false ;
	
	var l = $('input[name="language"]').val() ;
	if ( l.length < 2 ) return false ;
	var p = $('input[name="project"]').val() ;
	if ( l == 'wikidata' ) { l = 'www' ; p = 'wikidata' ; }
	var lp = l+'.'+p ;
	if ( lp == last_namespace_project ) return false ;
	
	function namespaceDataLoaded ( d ) {
		namespaces_loading = false ;
		if ( typeof d == 'undefined' ) return ;
		if ( typeof d.query == 'undefined' ) return ;
		if ( typeof d.query.namespaces == 'undefined' ) return ;
		last_namespaces = {} ;
		max_namespace_id = 0 ;
		$.each ( d.query.namespaces , function ( id , v ) {
			id *= 1 ;
			if ( id < 0 ) return ;
			var title = v['*'] ;
			if ( title == '' ) title = "<span class='l_namespace_0'></span>" ; //_t('namespace_0',l) ;
			last_namespaces[id] = [ title , (v.canonical||title) ] ;
			if ( id > max_namespace_id ) max_namespace_id = id ;
		} ) ;
		
		last_namespace_project = lp ;
		
		var nsl = [] ;
		function renderNS ( ns ) {
			var h = "<div>" ;
			if ( typeof last_namespaces[ns] != 'undefined' ) {
				h += "<label" ;
				if ( last_namespaces[ns][0] != last_namespaces[ns][1] ) h += " title='"+(last_namespaces[ns][1]||'')+"'" ;
				h += "><input type='checkbox' value='1' ns='"+ns+"' name='ns["+ns+"]'" ;
				if ( $.inArray ( ns , namespaces_selected ) >= 0 ) {
					nsl.push ( ns ) ;
					h += " checked" ;
				}
				h += "> " ;
				h += last_namespaces[ns][0] ;
				h += "</label>" ;
			} else h += "&mdash;" ;
			h += "</div>" ;
			return h ;
		}
		
		$(".current_wiki").text ( lp ) ;
		h = "" ;
		h += "<div class='smaller'>" ;
		for ( var ns = 0 ; ns <= max_namespace_id ; ns += 2 ) {
			if ( typeof last_namespaces[ns] == 'undefined' && typeof last_namespaces[ns+1] == 'undefined' ) continue ;
			h += "<div class='ns-block'>" ;
			h += renderNS ( ns ) ;
			h += renderNS ( ns+1 ) ;
			h += "</div>" ;
		}
		h += "</div>" ;
		namespaces_selected = nsl ;
		
		$('#namespaces').html ( h ) ;
		$('#namespaces input').change ( function () {
			var o = $(this) ;
			var ns = o.attr('ns') * 1 ;
			namespaces_selected = jQuery.grep(namespaces_selected, function(value) { return value != ns; }); // Remove is present
			if ( o.is(':checked') ) namespaces_selected.push ( ns ) ;
		} ) ;
		
	}
	
	var server = lp+'.org' ;
	if ( typeof global_namespace_cache[server] == 'undefined' ) {
		namespaces_loading = true ;
		$.getJSON ( 'https://'+server+'/w/api.php?action=query&meta=siteinfo&siprop=namespaces&format=json&callback=?' , function ( d ) {
			global_namespace_cache[server] = d ;
			namespaceDataLoaded ( d ) ;
		} ) . always ( function () {
			namespaces_loading = false ;
			if ( typeof callback != 'undefined' ) callback() ;
		} ) ;
	} else {
		namespaceDataLoaded ( global_namespace_cache[server] ) ;
		namespaces_loading = false ;
		if ( typeof callback != 'undefined' ) callback() ;
	}
	return false ;
}

var autolist ;


// Converts a WDQ input box to SPARQL via wdq2sparql, if possible
function wdq2sparql ( wdq , sparql_selector ) {
	$(sparql_selector).prop('disabled', true);

	$.get ( '/wdq2sparql/?' + encodeURIComponent(wdq) , function ( d ) {
		$(sparql_selector).prop('disabled', false);
		if ( d.status != 'OK' ) {
			alert ( _t('wdq2sparql_fail') ) ;
			return ;
		}
		var sparql = $.trim ( d.sparql ) ;
		sparql = sparql.replace ( /^prefix.+$/mig , '' ) ;
		sparql = sparql.replace ( /\s+/g , ' ' ) ;
		sparql = sparql.replace ( /\s*\.\s*\}\s*$/g , ' }' ) ;
		sparql = $.trim ( sparql ) ;
		$(sparql_selector).val ( sparql ) ;
	} , 'json' ) ;


/*	
	$.get ( 'https://tools.wmflabs.org/wdq2sparql/w2s.php' , {
		wdq:wdq
	} , function ( d ) {
		$(sparql_selector).prop('disabled', false);
		if ( d.match ( /^<!DOCTYPE/ ) ) {
			alert ( _t('wdq2sparql_fail') ) ;
			return ;
		}
		d = d.replace ( /^prefix.+$/mig , '' ) ;
		d = d.replace ( /\s+/g , ' ' ) ;
		d = $.trim ( d ) ;
		$(sparql_selector).val ( d ) ;
	} ) ;
*/	
	return false ;
}

function loadThumbnail ( img ) {
//	if ( img.css('display') === 'none' ) return ; // Hacking around Chrome bug; should be: if ( !img.is(':visible') ) return ;
	if ( !img.is_on_screen() ) return ;
	img.removeClass ( 'pre_thumb' ) ;
	img.attr ( { src : img.attr('src2load') } ) ;
}

function generateThumbnailView() {
	var h = "<div id='thumbnails' class='card-columns'></div>" ;
	$('#main_table').after ( h ) ;
	
	$('#main_table tbody tr').each ( function () {
		var tr = $(this) ;
		var td = $(tr.find('td.link_container').get(0)) ;
		var a = $(td.find('a.pagelink').get(0)) ;
		var url = a.attr('href').replace(/\/wiki\/File%3A/,'/wiki/Special:Redirect/file/') + '?width=' + thumbnail_size ;
		var h = '' ;
		h += '<div class="card">' ;
		h += '<div style="text-align:center"><a class="thumblink"><img class="card-img-top pre_thumb" src2load="'+url+'" alt="'+'??'+'" border=0 /></a></div>' ;
		h += '<div class="card-block">' ;
		h += '<p class="card-text">' ;
		h += $('<div>').append(a.clone()).html();
		h += '</p>' ;
		h += '</div>' ;
		h += '</div>' ;
		var card = $(h) ;
		card.find('a.thumblink').attr({href:a.attr('href'),target:'_blank'}) ;
		$('#thumbnails').append ( card ) ;
	} ) ;

	// Load thumbnails on demand
	$(window).scroll(function(){
		$('#thumbnails img.pre_thumb').each ( function () {
			loadThumbnail ( $(this) ) ;
		} ) ;
	} ) ;
}

function validateSourceCombination () {
	var o = $('input[name="source_combination"]') ;
	var text = $.trim ( o.val() ) ;
	var reg = /^( and | or | not |categories|sparql|pagepile|manual|\(|\)| )*$/gi ;
	var op = $(o.parents("div.input-group").get(0)) ;
	if ( reg.test ( text ) ) {
		op.removeClass ( 'has-danger' ) ;
		$('#doit').prop('disabled', false)
	} else {
		op.addClass ( 'has-danger' ) ;
		$('#doit').prop('disabled', true)
	}
	return false ;
}

function initializeInterface () {
	var p = getUrlVars() ;
	

	// Ensure NS0 is selected by default
	var cnt = 0 ;
	$.each ( p , function ( k , v ) { cnt++ } ) ;
	if ( cnt == 0 ) p['ns[0]'] = 1 ;
	
	// Legacy parameters
	if ( typeof p.category != 'undefined' && (p.categories||'') == '' ) p.categories = p.category ;
	if ( typeof p.cats != 'undefined' && (p.categories||'') == '' ) p.categories = p.cats ;
	if ( typeof p.wdqs != 'undefined' ) p.sparql = p.wdqs ;
	if ( typeof p.statementlist != 'undefined' ) p.al_commands = p.statementlist ;
	if ( typeof p.comb_subset != 'undefined' ) p.combination = 'subset' ;
	if ( typeof p.comb_union != 'undefined' ) p.combination = 'union' ;
	if ( typeof p.get_q != 'undefined' ) p.wikidata_item = 'any' ;
	if ( typeof p.wikidata != 'undefined' ) p.wikidata_item = 'any' ;
	if ( typeof p.wikidata_no_item != 'undefined' ) p.wikidata_item = 'without' ;
	if ( typeof p.giu != 'undefined' ) p.file_usage_data = 'on' ;
	
	

	params = $.extend ( {} , default_params , p ) ;
	
	var l = 'en' ;
	if ( typeof params.interface_language != 'undefined' ) l = params.interface_language ;
	
	loadInterface ( l , function () {
		if ( typeof window.AutoList != 'undefined' ) autolist = new AutoList () ;
	} ) ;
	$('input[name="language"]').focus() ;
/*	$('#doit').click ( function () {
		$('#main_form').submit() ;
	} ) ;*/
	
	$('#main_form ul.nav-tabs a').click ( function (e) {
		e.preventDefault() ;
		var o = $(this) ;
		$('input[name="active_tab"]').val ( o.attr('href').replace(/^\#/,'') ) ;
	} ) ;
	
	$('#wdq2sparql').click ( function ( e ) {
		e.preventDefault() ;
		var wdq = prompt ( _t('wdq2sparql_prompt') , '' ) ;
		if ( wdq == null || $.trim(wdq) == '' ) return false ;
		wdq2sparql ( wdq , 'textarea[name="sparql"]' ) ;
		return false ;
	} ) ;
	
	$('input[name="source_combination"]').keyup ( validateSourceCombination ) ;
	
	$('#file_results label').change ( function () {
		var o = $('#file_results input[name="results_mode"]:checked') ;
		var mode = o.val() ;
		
		if ( mode == 'titles' ) {
			$('#thumbnails').hide() ;
			$('#main_table').show() ;
			return ;
		}
		
		// mode == 'thumbnails'

		if ( $('#thumbnails').length == 0 ) {
			generateThumbnailView() ;
		}

		$('#main_table').hide() ;
		$('#thumbnails').show() ;
		$(window).scroll() ;
	} ) ;
	
	function highlightMissingWiki () {
		var o = $('textarea[name="manual_list"]') ;
		var wo = $('input[name="manual_list_wiki"]') ;
		var text = o.val() ;
		var wiki = wo.val() ;
		var wop = $(wo.parents("div.input-group").get(0)) ;
		if ( $.trim(text) != '' && !wiki.match(/wiki\s*$/) ) { //$.trim(wiki) == '' ) {
			wop.addClass ( 'has-danger' ) ;
			$('#doit').prop('disabled', true)
		} else {
			wop.removeClass ( 'has-danger' ) ;
			$('#doit').prop('disabled', false)
		}
	}
	$('textarea[name="manual_list"]').keyup ( highlightMissingWiki ) ;
	$('input[name="manual_list_wiki"]').keyup ( highlightMissingWiki ) ;
	highlightMissingWiki() ;
	
	$('#tab-list').click ( function () {
		if ( $('#main_form div.tab-pane').length > 0 ) {
			$('#tab-list').text ( _t('toggle_tabs') ) ;
			$('#main_form ul.nav-tabs').hide() ;
			$('#main_form div.tab-pane').addClass('former-tab-pane').removeClass('tab-pane') ;
		} else {
			$('#tab-list').text ( _t('toggle_list') ) ;
			$('#main_form ul.nav-tabs').show() ;
			$('#main_form div.former-tab-pane').addClass('tab-pane').removeClass('former-tab-pane') ;
		}
	} ) ;
	
	// Deactivate REDIRECTS when showing only pages without item. This will be a mess otherwise, few users would think to change that setting. It can always be changed back manually.
	$('input[name="wikidata_item"][value="without"]').click ( function ( e ) {
		$('input[name="show_redirects"][value="no"]').prop('checked', true);
	} ) ;
	
	$('#quick_commons').click ( function ()     { $("input[name='language']").val("commons");  $("input[name='project']").val("wikimedia"); loadNamespaces() ; return false } ) ;
	$('#quick_wikispecies').click ( function () { $("input[name='language']").val("species");  $("input[name='project']").val("wikimedia"); loadNamespaces() ; return false } ) ;
	$('#quick_wikidata').click ( function ()    { $("input[name='language']").val("wikidata"); $("input[name='project']").val("wikimedia"); loadNamespaces() ; return false } ) ;

}

$(document).ready ( function () {
	initializeInterface() ;
} ) ;