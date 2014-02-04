//===== Hercules Plugin ======================================
//= @security
//===== By: ==================================================
//= Samuel
//===== Current Version: =====================================
//= 1.1
//===== Compatible With: ===================================== 
//= Hercules
//===== Description: =========================================
//= Allow to disable/enable item transfers
//===== Additional Comments: =================================  
// 1.0.0 Release
// 1.1.0 Added support for Hercules 2014 Megapatch
//============================================================

#include <stdio.h>
#include <string.h>
#include "../common/db.h"
#include "../common/HPMi.h"
#include "../common/malloc.h"
#include "../common/mmo.h"
#include "../common/strlib.h"
#include "../map/atcommand.h"
#include "../map/buyingstore.h"
#include "../map/clif.h"
#include "../map/map.h"
#include "../map/npc.h"
#include "../map/pc.h"
#include "../map/script.h"

HPExport struct hplugin_info pinfo = {
	"security",		// Plugin name
	SERVER_TYPE_MAP,// Which server types this plugin works with?
	"0.1",			// Plugin version
	HPM_VERSION,	// HPM Version (don't change, macro is automatically updated)
};

struct security_data_struct {
	unsigned int secure_items : 1; // [Zephyrus] Item Security
};

struct security_data_struct *get_security_variable(struct map_session_data *sd) {
	struct security_data_struct *data;
	
	// Let's check if we already inserted the field
	if( !(data = getFromMSD(sd,0)) ) {
		
		CREATE(data,struct security_data_struct, 1);
		
		addToMSD(sd, data, 0, true);
	}

	return data;
}

/*=========================================
 * Item Security System
 *-----------------------------------------*/
ACMD(security)
{
	struct security_data_struct *my = get_security_variable(sd);
	if (!sd) return false;

	if( sd->npc_id || sd->vender_id || sd->buyer_id || sd->state.trading || sd->state.storage_flag )
		return false;

	npc->event(sd,"SecuritySystem::OnSettings",0);
	return true;
}

/*==========================================
 * Item Security System
 *------------------------------------------*/
BUILDIN(setsecurity)
{
	struct map_session_data *sd = script->rid2sd(st);
	struct security_data_struct *my = get_security_variable(sd);
	int value = script_getnum(st,2);
	if( sd == NULL )
		return true;

	my->secure_items = (value)?1:0;
	return true;
}

BUILDIN(getsecurity)
{
	struct map_session_data *sd = script->rid2sd(st);
	struct security_data_struct *my = get_security_variable(sd);
	if( sd == NULL )
		return true;

	script_pushint(st,my->secure_items);
	return true;
}

bool my_buyingstore_setup(struct map_session_data* sd, unsigned char slots) {
	struct security_data_struct *my = get_security_variable(sd);

	if( my->secure_items )
	{
		clif->message(sd->fd, "You can't open Buying. Blocked with @security");
	}
	return false;
}

void pre_clif_parse_NpcBuyListSend(int fd, struct map_session_data* sd)
{
	struct security_data_struct *my = get_security_variable(sd);
	int result;

	if( my->secure_items ){
		clif->message(sd->fd, "You can't buy. Blocked with @security");
		result = 1;
		sd->npc_shopid = 0; //Clear shop data.
	}
}

void my_clif_parse_NpcSellListSend(int fd,struct map_session_data *sd)
{
	struct security_data_struct *my = get_security_variable(sd);
	int fail=0;

	if( my->secure_items )
	{
		clif->message(sd->fd, "You can't sell. Blocked with @security");
		fail = 1;
		sd->npc_shopid = 0; //Clear shop data.
	}
}

void my_clif_parse_OpenVending(int fd, struct map_session_data* sd)
{
	struct security_data_struct *my = get_security_variable(sd);

	if( my->secure_items )
	{
		clif->message(sd->fd, "You can't open vending. Blocked with @security");
		sd->state.prevend = sd->state.workinprogress = 0;
	}
	return;
}

int my_pc_dropitem(struct map_session_data *sd,int n,int *amount)
{
	struct security_data_struct *my = get_security_variable(sd);

	if( my->secure_items ) {
		clif->message(sd->fd, "You can't drop. Blocked with @security");
		*amount = 0;
	}
	return 1;
}

/*==========================================
 * Invoked once after the char/account/account2 registry variables are received. [Skotlex]
 *------------------------------------------*/
int my_pc_reg_received(struct map_session_data *sd)
{
	struct security_data_struct *my = get_security_variable(sd);
	
	sd->vars_ok = true;
// Security System
	if( pc_readaccountreg(sd,script->add_str("#SECURITYCODE")) > 0 )
	{
		clif->message(sd->fd, "Item Security System ENABLE : Use @security for more options.");
		my->secure_items = 1;
	}
	else
		clif->message(sd->fd, "Item Security System DISABLE : Use @security for more options.");
	
	return 1;
}

int my_guild_storage_additem(struct map_session_data* sd, struct guild_storage* stor, struct item* item_data, int *amount)
{
	struct security_data_struct *my = get_security_variable(sd);

	if( my->secure_items )
	{
		clif->message(sd->fd, "You can't store items on Guild Storage. Blocked with @security");
		*amount = 0;
	}
	return 1;
}

/*==========================================
 * Initiates a trade request.
 *------------------------------------------*/
void my_trade_traderequest(struct map_session_data *sd, struct map_session_data *target_sd)
{
	struct security_data_struct *my = get_security_variable(sd);
	struct security_data_struct *trial = get_security_variable(target_sd);

	if( my->secure_items ) {
		clif->message(sd->fd, "You can't trade. Blocked with @security");
		target_sd->trade_partner = 1;
		sd->trade_partner = 1;
		return;
	}

	if( trial->secure_items ) {
		clif->message(sd->fd, "Target can't trade. Blocked with @security");
		target_sd->trade_partner = 1;
		sd->trade_partner = 1;
		return;
	}

}

HPExport void plugin_init (void) {
	iMalloc = GET_SYMBOL("iMalloc");
	atcommand = GET_SYMBOL("atcommand");
	clif = GET_SYMBOL("clif");
	npc = GET_SYMBOL("npc");
	script = GET_SYMBOL("script");
	map = GET_SYMBOL("map");
	pc = GET_SYMBOL("pc");

	/* hooks */
	addHookPre("buyingstore->setup",my_buyingstore_setup);
	
	/* clif */
	addHookPre("clif->pNpcBuyListSend",pre_clif_parse_NpcBuyListSend);
	addHookPre("clif->pNpcSellListSend",my_clif_parse_NpcSellListSend);
	addHookPre("clif->pOpenVending",my_clif_parse_OpenVending);

	addHookPre("pc->reg_received",my_pc_reg_received);
	addHookPre("pc->dropitem",my_pc_dropitem);

	addHookPre("gstorage->additem",my_guild_storage_additem);

	addHookPre("trade->request",my_trade_traderequest);
	
	addAtcommand("security",security);

	addScriptCommand("setsecurity","i",setsecurity);
	addScriptCommand("getsecurity","",getsecurity);
}
