function WiDaR ( callback ) {
	
	this.is_logged_in = false ;
	this.server = 'https://tools.wmflabs.org' ;
	this.api = this.server+'/widar/index.php?callback2=?' ;
	this.userinfo = {} ;
	this.tool_hashtag = '' ;
	
	this.isLoggedIn = function () {
		return this.is_logged_in ;
	}
	
	this.getInfo = function () {
		var me = this ;
		this.abstractCall ( { 
			action:'get_rights'
		} , function ( d ) {
			me.is_logged_in = false ;
			me.userinfo = {} ;
			if ( typeof (((d||{}).result||{}).query||{}).userinfo == 'undefined' ) {
				callback() ;
				return ;
			}
			me.userinfo = d.result.query.userinfo ;
			if ( typeof me.userinfo.name != 'undefined' ) me.is_logged_in = true ;
			callback() ;
		} ) ;
	}
	
	this.getLoginLink = function ( text ) {
		var h = "<a target='_blank' href='"+this.server+"/widar/index.php?action=authorize'>" + text + "</a>" ;
		return h ;
	}
	
	this.isBot = function () {
		if ( !this.isLoggedIn() ) return false ;
		if ( typeof this.userinfo.groups == 'undefined' ) return false ;
		return -1 != $.inArray ( 'bot' , this.userinfo.groups ) ;
	}
	
	this.getUserName = function () {
		if ( !this.isLoggedIn() ) return ;
		return this.userinfo.name ;
	}
	
	this.abstractCall = function ( params , callback ) {
		var me = this ;
		params.tool_hashtag = me.tool_hashtag ;
		params.botmode = 1 ;
		$.getJSON ( me.api , params , function ( d ) {
			if ( typeof callback != 'undefined' ) callback ( d ) ;
		} ) . fail ( function () {
			if ( typeof callback != 'undefined' ) callback () ;
		} ) ;
	}
	
	this.genericAction = function ( o , callback ) {
		this.abstractCall ( { 
			action:'generic',
			json:JSON.stringify(o)
		} , callback ) ;
	}
	
	this.removeClaim = function ( id , callback ) {
		this.abstractCall ( {
			action:'remove_claim',
			id:id
		} , callback ) ;
	}
	
	this.createItemForPage = function ( page , wiki , callback ) {
		this.abstractCall ( {
			action:'create_item_from_page',
			botmode:1,
			site:wiki,
			page:page
		} , callback ) ;
	}
	
	this.setClaim = function ( q , prop , target_q , callback ) {
		this.abstractCall ( { 
			action:'set_claims',
			ids:q,
			prop:prop,
			target:target_q
		} , callback ) ;
	}
	
	
	this.getInfo() ;
}

//________________________________________________________________________________________________________________________________________________

function AutoList ( callback ) {

	this.running = [] ;
	this.max_concurrent = 1 ; // 1 thread for non-bot user
	this.delay = 2000 ; // 2 sec delay for non-bot user

	this.setupCommands = function () {
		var me = this ;
		me.commands_todo = [] ;
		me.running = [] ;
		var rows = $('#al_commands').val().split("\n") ;
		$('#main_table input.qcb').each ( function () {
			var o = $(this) ;
			if ( !o.is(':checked') ) return ;
			var tr = $(o.parents('tr').get(0)) ;
			var q = o.attr('q') ;
			var remove_q ;
			
			if ( me.mode == 'creator' ) {
				var cmd = { q:q , status:'waiting' , mode:'create' } ;
				cmd.page = $($(tr.find("td").get(2)).find('a').get(0)).attr('href').replace(/^.+?\/wiki\//,'') ;
				remove_q = me.commands_todo.length ;
				me.commands_todo.push ( cmd ) ;
			}
			
			$.each ( rows , function ( k , row ) {
				var cmd = { q:q , status:'waiting' } ;
				var m = row.match ( /^\s*-(P\d+)/i ) ;
				if ( m == null ) {
					m = row.match ( /^\s*(P\d+)\s*:\s*(Q\d+)\s*$/i ) ;
					if ( m != null ) {
						cmd.mode = 'add' ;
						cmd.prop = m[1] ;
						cmd.value = m[2] ;
					} else return ;
				} else { // Delete property
					cmd.mode = 'delete' ;
					cmd.prop = m[1] ;
					m = row.match ( /^\s*-(P\d+)\s*:\s*Q(\d+)/i ) ;
					if ( m != null ) cmd.value = m[2] ;
				}
				remove_q = me.commands_todo.length ;
				me.commands_todo.push ( cmd ) ;
			} ) ;
			if ( typeof remove_q != 'undefined' ) {
				me.commands_todo[remove_q].remove_q = true ;
				me.commands_todo[remove_q].cb_id = o.attr('id') ;
			}
		} ) ;
	}
	
	this.finishCommand = function ( id ) {
		var me = this ;
		me.commands_todo[id].status = 'done' ;
		var cmd = me.commands_todo[id] ;
		
		if ( cmd.remove_q ) { // Last for this q, remove row
			$($('#'+cmd.cb_id).parents('tr').get(0)).remove() ;
		}
		
		var tmp = [] ;
		$.each ( me.running , function ( k , v ) {
			if ( v != id ) tmp.push ( v ) ;
		} ) ;
		me.running = tmp ;
		
		setTimeout ( function () { me.runNextCommand() } , me.delay ) ;
	}
	
	this.addNewQ = function ( q ) {
		if ( $('#autolist_box_new_q').length == 0 ) {
			$('#autolist_box').append ( "<div class='autolist_subbox'><div class='l_created_items'>"+_t('created_items')+"</div><textarea id='autolist_box_new_q' rows='4' style='width:80px;font-size:8pt'></textarea></div>" ) ;
		}
		var t = $('#autolist_box_new_q').val() ;
		if ( t != '' ) t += "\n" ;
		t += "Q" + q ;
		$('#autolist_box_new_q').val(t) ;
	}
	
	this.runCommand = function ( id ) {
		var me = this ;
		me.running.push ( id ) ;
		me.commands_todo[id].status = 'running' ;
		var cmd = me.commands_todo[id] ;
		
		if ( cmd.mode == 'create' ) {
		
			me.widar.createItemForPage ( cmd.page , output_wiki , function ( d ) {
				// TODO error check: d.error=="OK"
				
				// Update all subsequent commands for this item to use the new Q
				var new_q = d.q.replace(/\D/,'') ;
				var old_q = cmd.q ;
				me.addNewQ ( new_q ) ;
				$.each ( me.commands_todo , function ( k , v ) {
					if ( v.q == old_q ) me.commands_todo[k].q = new_q ;
				} ) ;
				
				// Next
				me.finishCommand ( id ) ;
			} ) ;
		
		} else if ( cmd.mode == 'add' ) {
			me.widar.setClaim ( 'Q'+cmd.q , cmd.prop , cmd.value , function ( d ) {
				// TODO error log
				me.finishCommand ( id ) ;
			} ) ;
		} else if ( cmd.mode == 'delete' ) {
			$.getJSON ( 'https://www.wikidata.org/w/api.php?action=wbgetentities&ids=Q'+cmd.q+'&format=json&callback=?' , function ( d ) {
				var done = false ;
				$.each ( ((((d.entities||{})['Q'+cmd.q]||{}).claims||{})[cmd.prop.toUpperCase()]||{}) , function ( k , v ) {
					if ( typeof cmd.value != 'undefined' ) { // Specific value to delete
						if ( ((((v.mainsnak||{}).datavalue||{}).value||{})['numeric-id']||'') != cmd.value ) return ;
					}
					done = true ;
					me.widar.removeClaim ( v.id , function ( d ) {
						// TODO error log
						console.log ( d ) ;
						me.finishCommand ( id ) ;
					} ) ;
					return false ; // Remove just the first one
				} ) ;
				if ( !done ) {
					// TODO error log
					me.finishCommand ( id ) ;
				}
			} ) . fail ( function () {
				// TODO error log
				me.finishCommand ( id ) ;
			} ) ;
		} else {
			
		}
	}
	
	this.allDone = function () {
		$('#al_do_stop').hide() ;
		$('#al_do_process').show() ;
		$('#al_status').html ( "<span style='color:green'>"+_t('al_done')+"</span>" ) ;
	}

	this.runNextCommand = function () {
		var me = this ;
		
		if ( me.emergency_stop ) return ; // Used clicked stop

		if ( me.running.length >= me.max_concurrent ) {
			setTimeout ( function () { me.runNextCommand } , 100 ) ; // Was already called, so short delay
			return ;
		}
		
		var q_running = {} ;
		$.each ( me.running , function ( k , v ) {
			q_running[me.commands_todo[v].q] = 1 ;
		} ) ;

		var run_next ;
		var has_commands_left = false ;
		$.each ( me.commands_todo , function ( k , v ) {
			if ( v.status != 'done' ) has_commands_left = true ;
			if ( v.status != 'waiting' ) return ; // Already running
			if ( typeof q_running[v.q] != 'undefined' ) return ; // Don't run the same q multiple times
			run_next = k ;
			return false ;
		} ) ;

		if ( !has_commands_left ) {
			me.allDone() ;
			return ;
		}
		
		if ( typeof run_next == 'undefined' ) {
			setTimeout ( function () { me.runNextCommand } , 100 ) ; // Was already called, so short delay
			return ;
		}

		me.updateRunCounter() ;
		me.runCommand ( run_next ) ;
	}
	
	this.updateRunCounter = function () {
		var me = this ;
		var t = _t('al_running') ;
		var all = me.commands_todo.length ;
		var cnt = 0 ;
		$.each ( me.commands_todo , function ( k , v ) {
			if ( v.status != 'done' ) cnt++ ;
		} ) ;
		t = t.replace ( '$1' , cnt ) ;
		t = t.replace ( '$2' , all ) ;
		$('#al_status').html ( t ) ;
	}

	this.initializeAutoListBox = function () {
		var me = this ;
		var h = '' ;
		var p = getUrlVars() ;
		if ( me.widar.isLoggedIn() ) {
			h += "<div class='autolist_subbox'>" ;
			h += "<div>Welcome, " + me.widar.getUserName() + "!</div>" ;
			if ( me.mode == "creator" ) {
				h += "<div class='l_al_creator_mode'></div>" ;
			}
			if ( me.widar.isBot() ) {
				h += "<div class='l_al_bot'></div>" ;
				me.max_concurrent = 5 ;
				me.delay = 1 ;
			}
			h += "</div>" ;
			h += "<div class='autolist_subbox'>" ;
			h += "<button id='al_do_check_all' class='btn btn-secondary btn-sm l_al_all'></button><br/>" ;
			h += "<button id='al_do_check_none' class='btn btn-secondary btn-sm l_al_none'></button><br/>" ;
			h += "<button id='al_do_check_toggle' class='btn btn-secondary btn-sm l_al_toggle'></button><br/>" ;
			h += "</div>" ;
			h += "<div class='autolist_subbox'>" ;
			h += "<textarea id='al_commands' class='ph_al_commands_ph' rows=3 style='width:200px'>" + (p.statementlist||'') + "</textarea><br/>" ;
			h += "<button id='al_do_process' class='btn btn-success btn-sm l_al_process'></button>" ;
			h += "<button id='al_do_stop' class='btn btn-danger btn-sm l_al_stop' style='display:none'></button>" ;
			h += "<div id='al_status'></div>" ;
			h += "</div>" ;
		} else {
			h += "<div>" + me.widar.getLoginLink("<span class='l_al_login'></span>") + "</div>" ;
		}
		$('#autolist_box').html ( h ) ;
		
		if ( typeof p.al_commands != 'undefined' ) {
			$('#al_commands').val ( p.al_commands ) ;
			me.commandsHaveChanged() ;
		}
		
		me.setInterfaceLanguage ( interface_language ) ;
	
	
		$('#al_do_process').click ( function (e) {
			e.preventDefault() ;
			me.emergency_stop = false ;
			$('#al_do_process').hide() ;
			$('#al_do_stop').show() ;
			me.setupCommands() ;
			me.updateRunCounter() ;

			for ( var i = 0 ; i < me.max_concurrent ; i++ ) {
				me.runNextCommand() ;
			}
		} ) ;

		$('#al_do_stop').click ( function (e) {
			e.preventDefault() ;
			me.emergency_stop = true ;
			$('#al_do_process').show() ;
			$('#al_do_stop').hide() ;
			$('#al_status').html ( "<span style='color:red;'>" + _t('al_stopped') + "</span>" ) ;
		} ) ;

		$('#al_do_check_all').click ( function (e) {
			e.preventDefault() ;
			$('#main_table input.qcb').prop('checked',true) ;
		} ) ;

		$('#al_do_check_none').click ( function (e) {
			e.preventDefault() ;
			$('#main_table input.qcb').prop('checked',false) ;
		} ) ;

		$('#al_do_check_toggle').click ( function (e) {
			e.preventDefault() ;
			$('#main_table input.qcb').each ( function () {
				var o = $(this) ;
				o.prop ( 'checked' , !o.prop('checked') ) ;
			} ) ;
		} ) ;

		$('#al_commands').keyup ( function () {
			me.commandsHaveChanged() ;
		} ) ;
		
	}
	
	this.commandsHaveChanged = function () {
		$('#main_form input[name="al_commands"]').remove() ;
		$('#main_form').append ( "<input type='hidden' name='al_commands' />" ) ;
		var v = $('#al_commands').val() ;
		$('#main_form input[name="al_commands"]').val ( v ) ;
		$('#permalink a').each ( function () {
			var a = $(this) ;
			var href = a.attr('href') ;
			href = href.replace ( /&al_commands=[^&]+/ , '' ) ;
			href += '&al_commands=' + encodeURIComponent ( v ) ;
			href = href.replace ( /&{2,}/ , '&' ) ;
			a.attr ( { href : href } ) ;
		} ) ;
	}

	this.setInterfaceLanguage = function ( l ) {
		$.each ( interface_text['en'] , function ( k , v ) {
			if ( !k.match(/^al_/) ) return ; // AutoList only
			if ( typeof interface_text[l][k] != 'undefined' ) v = interface_text[l][k] ;
			$('#autolist_box .l_'+k).html ( v ) ;
			$('#autolist_box .ph_'+k).attr ( { placeholder:v } ) ;
		} ) ;
	}
	
	this.addCheckLinks = function () {
		var me = this ;
		if ( me.mode != 'creator' ) return ;
		$('#main_table tbody tr').each ( function () {
			var tr = $(this) ;
			var td = $(tr.find('td').get(2)) ;
			var page = $(td.find('a').get(0)).attr('href').replace(/^.+?\/wiki\//,'').replace(/_/,' ') ;
			td.append ( " <span class='pull-xs-right smaller'>[<a class='l_check_wd' target='_blank'>"+_t('check_wd')+"</a>]</span>" ) ;
			td.find('a.l_check_wd').attr({href:'https://tools.wmflabs.org/wikidata-todo/duplicity.php?wiki='+output_wiki+'&norand=1&page='+page}) ;
		} ) ;
	}

	var me = this ;
	if ( $('#autolist_box').length == 0 || $('#main_table input.qcb').length == 0 ) { // Don't bother
		if ( typeof callback != 'undefined' ) callback() ;
		return ;
	}

	me.mode = $('#autolist_box').attr('mode') ;
	me.commands_todo = [] ;
	me.addCheckLinks () ;
	me.widar = new WiDaR ( function () {
		me.widar.tool_hashtag = 'petscan' ;
		me.initializeAutoListBox() ;
		if ( typeof callback != 'undefined' ) callback() ;
	} ) ;
}

