
#include "script/ScriptedPlayer.h"

#include "core/Core.h"
#include "game/Player.h"
#include "game/Inventory.h"
#include "graphics/Math.h"
#include "graphics/data/Mesh.h"
#include "graphics/particle/ParticleEffects.h"
#include "gui/Interface.h"
#include "io/FilePath.h"
#include "scene/Interactive.h"
#include "scene/GameSound.h"
#include "script/ScriptEvent.h"
#include "script/ScriptUtils.h"

using std::string;

extern float InventoryDir;
extern INTERACTIVE_OBJ * CURRENT_TORCH;

namespace script {

namespace {

class AddBagCommand : public Command {
	
public:
	
	AddBagCommand() : Command("addbag") { }
	
	Result execute(Context & context) {
		
		ARX_UNUSED(context);
		
		DebugScript("");
		
		ARX_PLAYER_AddBag();
		
		return Success;
	}
	
};

class AddXpCommand : public Command {
	
public:
	
	AddXpCommand() : Command("addxp") { }
	
	Result execute(Context & context) {
		
		float val = context.getFloat();
		
		DebugScript(' ' << val);
		
		ARX_PLAYER_Modify_XP(static_cast<long>(val));
		
		return Success;
	}
	
};

class AddGoldCommand : public Command {
	
public:
	
	AddGoldCommand() : Command("addgold") { }
	
	Result execute(Context & context) {
		
		float val = context.getFloat();
		
		DebugScript(' ' << val);
		
		if(val != 0) {
			ARX_SOUND_PlayInterface(SND_GOLD);
		}
		
		ARX_CHECK_LONG(val);
		ARX_PLAYER_AddGold(static_cast<long>(val));
		
		return Success;
	}
	
};

class RidiculousCommand : public Command {
	
public:
	
	RidiculousCommand() : Command("ridiculous") { }
	
	Result execute(Context & context) {
		
		ARX_UNUSED(context);
		
		DebugScript("");
		
		ARX_PLAYER_MakeFreshHero();
		
		return Success;
	}
	
};

class RuneCommand : public Command {
	
	typedef std::map<string, RuneFlag> Runes;
	Runes runes;
	
public:
	
	RuneCommand() : Command("rune") {
		runes["aam"] = FLAG_AAM;
		runes["cetrius"] = FLAG_CETRIUS;
		runes["comunicatum"] = FLAG_COMUNICATUM;
		runes["cosum"] = FLAG_COSUM;
		runes["folgora"] = FLAG_FOLGORA;
		runes["fridd"] = FLAG_FRIDD;
		runes["kaom"] = FLAG_KAOM;
		runes["mega"] = FLAG_MEGA;
		runes["morte"] = FLAG_MORTE;
		runes["movis"] = FLAG_MOVIS;
		runes["nhi"] = FLAG_NHI;
		runes["rhaa"] = FLAG_RHAA;
		runes["spacium"] = FLAG_SPACIUM;
		runes["stregum"] = FLAG_STREGUM;
		runes["taar"] = FLAG_TAAR;
		runes["tempus"] = FLAG_TEMPUS;
		runes["tera"] = FLAG_TERA;
		runes["vista"] = FLAG_VISTA;
		runes["vitae"] = FLAG_VITAE;
		runes["yok"] = FLAG_YOK;
	}
	
	Result execute(Context & context) {
		
		long add = 0;
		HandleFlags("ar") {
			add = (flg & flag('r')) ? -1 : ((flg & flag('a')) ? 1 : 0);
		}
		
		string name = context.getWord();
		
		DebugScript(' ' << options << ' ' << name);
		
		if(name == "all") {
			
			if(add) {
				ScriptWarning << "unexpected flags: " << options << " all";
				return Failed;
			}
			
			ARX_PLAYER_Rune_Add_All();
			
		} else {
			
			if(!add) {
				ScriptWarning << "missing flags:  " << options << ' ' << name << "; expected -a or -r";
				return Failed;
			}
			
			Runes::const_iterator it = runes.find(name);
			if(it == runes.end()) {
				ScriptWarning << "unknown rune name: " << options << ' ' << name;
				return Failed;
			}
			
			if(add == 1) {
				ARX_Player_Rune_Add(it->second);
			} else if(add == -1) {
				ARX_Player_Rune_Remove(it->second);
			}
		}
		
		return Success;
	}
	
};

class QuestCommand : public Command {
	
public:
	
	QuestCommand() : Command("quest") { }
	
	Result execute(Context & context) {
		
		string name = loadUnlocalized(context.getWord());
		
		DebugScript(' ' << name);
		
		ARX_PLAYER_Quest_Add(name);
		
		return Success;
	}
	
};

class SetPlayerTweakCommand : public Command {
	
public:
	
	SetPlayerTweakCommand() : Command("setplayertweak", ANY_IO) { }
	
	Result execute(Context & context) {
		
		string command = context.getWord();
		
		INTERACTIVE_OBJ * io = context.getIO();
		if(!io->tweakerinfo) {
			io->tweakerinfo = (IO_TWEAKER_INFO *)malloc(sizeof(IO_TWEAKER_INFO));
			if(!(io->tweakerinfo)) {
				return Failed;
			}
			memset(io->tweakerinfo, 0, sizeof(IO_TWEAKER_INFO));
		}
		
		if(command == "skin") {
			
			string src = loadPath(context.getWord());
			string dst = loadPath(context.getWord());
			
			DebugScript(" skin " << src << ' ' << dst);
			
			strcpy(io->tweakerinfo->skintochange, src.c_str());
			strcpy(io->tweakerinfo->skinchangeto, dst.c_str());
			
		} else {
			
			string mesh = loadPath(context.getWord());
			
			DebugScript(" mesh " << mesh);
			
			strcpy(io->tweakerinfo->filename, mesh.c_str());
		}
		
		return Success;
	}
	
};

class SetHungerCommand : public Command {
	
public:
	
	SetHungerCommand() : Command("sethunger") { }
	
	Result execute(Context & context) {
		
		player.hunger = context.getFloat();
		
		DebugScript(' ' << player.hunger);
		
		return Success;
	}
	
};

class SetPlayerControlsCommand : public Command {
	
	static void Stack_SendMsgToAllNPC_IO(ScriptMessage msg, const char * dat) {
		for(long i = 0; i < inter.nbmax; i++) {
			if(inter.iobj[i] && (inter.iobj[i]->ioflags & IO_NPC)) {
				Stack_SendIOScriptEvent(inter.iobj[i], msg, dat);
			}
		}
	}
	
public:
	
	SetPlayerControlsCommand() : Command("setplayercontrols") { }
	
	Result execute(Context & context) {
		
		INTERACTIVE_OBJ * oes = EVENT_SENDER;
		EVENT_SENDER = context.getIO();
		
		bool enable = context.getBool();
		
		DebugScript(' ' << enable);
		
		if(enable) {
			if(BLOCK_PLAYER_CONTROLS) {
				Stack_SendMsgToAllNPC_IO(SM_CONTROLS_ON, "");
			}
			BLOCK_PLAYER_CONTROLS = 0;
		} else {
			if(!BLOCK_PLAYER_CONTROLS) {
				ARX_PLAYER_PutPlayerInNormalStance(0);
				Stack_SendMsgToAllNPC_IO(SM_CONTROLS_OFF, "");
				ARX_SPELLS_FizzleAllSpellsFromCaster(0);
			}
			BLOCK_PLAYER_CONTROLS = 1;
			player.Interface &= ~INTER_COMBATMODE;
		}
		
		EVENT_SENDER = oes;
		
		return Success;
	}
	
};

class StealNPCCommand : public Command {
	
public:
	
	StealNPCCommand() : Command("stealnpc") { }
	
	Result execute(Context & context) {
		
		DebugScript("");
		
		if(player.Interface & INTER_STEAL) {
			SendIOScriptEvent(ioSteal, SM_STEAL, "off");
		}
		
		player.Interface |= INTER_STEAL;
		InventoryDir = 1;
		ioSteal = context.getIO();
		
		return Success;
	}
	
};

class SpecialFXCommand : public Command {
	
public:
	
	SpecialFXCommand() : Command("specialfx") { }
	
	Result execute(Context & context) {
		
		string type = context.getWord();
		
		INTERACTIVE_OBJ * io = context.getIO();
		
		if(type == "ylside_death") {
			DebugScript(" ylside_death");
			if(!io) {
				ScriptWarning << "can only use 'specialfx ylside_death' in IO context";
				return Failed;
			}
			SetYlsideDeath(io);
			
		} else if(type == "player_appears") {
			DebugScript(" player_appears");
			if(!io) {
				ScriptWarning << "can only use 'specialfx player_appears' in IO context";
				return Failed;
			}
			MakePlayerAppearsFX(io);
			
		} else if(type == "heal") {
			
			float val = context.getFloat();
			
			DebugScript(" heal " << val);
			
			if(!BLOCK_PLAYER_CONTROLS) {
				player.life += val;
			}
			player.life = clamp(player.life, 0.f, player.Full_maxlife);
			
		} else if(type == "mana") {
			
			float val = context.getFloat();
			
			DebugScript(" mana " << val);
			
			player.mana = clamp(player.mana + val, 0.f, player.Full_maxmana);
			
		} else if(type == "newspell") {
			
			context.skipWord();
			
			DebugScript(" newspell");
			
			MakeBookFX(DANAESIZX - INTERFACE_RATIO(35), DANAESIZY - INTERFACE_RATIO(148), 0.00001f);
			
		} else if(type == "torch") {
			
			DebugScript(" torch");
			
			if(!io || !(io->ioflags & IO_ITEM)) {
				ScriptWarning << "can only use 'specialfx torch' for items";
				return Failed;
			}
			
			INTERACTIVE_OBJ * ioo = io;
			if(io->_itemdata->count > 1) {
				ioo = CloneIOItem(io);
				MakeTemporaryIOIdent(ioo);
				ioo->show = SHOW_FLAG_IN_INVENTORY;
				ioo->scriptload = 1;
				ioo->_itemdata->count = 1;
				io->_itemdata->count--;
			}
			
			ARX_PLAYER_ClickedOnTorch(ioo);
			
		} else if(type == "fiery") {
			DebugScript(" fiery");
			if(!io) {
				ScriptWarning << "can only use 'specialfx fiery' in IO context";
				return Failed;
			}
			io->ioflags |= IO_FIERY;
			
		} else if(type == "fieryoff") {
			DebugScript(" fieryoff");
			if(!io) {
				ScriptWarning << "can only use 'specialfx fieryoff' in IO context";
				return Failed;
			}
			io->ioflags &= ~IO_FIERY;
			
		} else if(type == "torchon") {
			DebugScript(" torchon");
			// do nothing
			
		} else if(type == "torchoff") {
			DebugScript(" torchoff");
			if(CURRENT_TORCH) {
				ARX_PLAYER_ClickedOnTorch(CURRENT_TORCH);
			}
			
		} else {
			ScriptWarning << "unknown fx: " << type;
			return Failed;
		}
		
		return Success;
	}
	
};

class KeyringAddCommand : public Command {
	
public:
	
	KeyringAddCommand() : Command("keyringadd") { }
	
	Result execute(Context & context) {
		
		string key = toLowercase(context.getStringVar(context.getWord()));
		
		DebugScript(' ' << key);
		
		ARX_KEYRING_Add(key);
		
		return Success;
	}
	
};

class PlayerLookAtCommand : public Command {
	
public:
	
	PlayerLookAtCommand() : Command("playerlookat") { }
	
	Result execute(Context & context) {
		
		string target = context.getWord();
		
		DebugScript(' ' << target);
		
		long t = GetTargetByNameTarget(target);
		if(t == -2) {
			t = GetInterNum(context.getIO());
		}
		if(!ValidIONum(t)) {
			ScriptWarning << "unknown target: " << target;
			return Failed;
		}
		
		ForcePlayerLookAtIO(inter.iobj[t]);
		
		return Success;
	}
	
};

class PrecastCommand : public Command {
	
public:
	
	PrecastCommand() : Command("precast") { }
	
	Result execute(Context & context) {
		
		SpellcastFlags spflags = SPELLCAST_FLAG_PRECAST | SPELLCAST_FLAG_NOANIM;
		bool dur = false;
		long duration = -1;
		HandleFlags("df") {
			if(flg & flag('d')) {
				spflags |= SPELLCAST_FLAG_NOCHECKCANCAST;
				duration = (long)context.getFloat();
				dur = 1;
			}
			if(flg & flag('f')) {
				spflags |= SPELLCAST_FLAG_NOCHECKCANCAST | SPELLCAST_FLAG_NOMANA;
			}
		}
		
		long level = clamp((long)context.getFloat(), 1l, 10l);
		
		string spellname = context.getWord();
		
		DebugScript(' ' << options << ' ' << duration << ' ' << level << ' ' << spellname);
		
		Spell spellid = GetSpellId(spellname);
		if(spellid == SPELL_NONE) {
			ScriptWarning << "unknown spell: " << spellname;
			return Failed;
		}
		
		if(!dur) {
			duration = 2000 + level * 2000;
		}
		
		if(context.getIO() != inter.iobj[0]) {
			spflags |= SPELLCAST_FLAG_NOCHECKCANCAST;
		}
		
		TryToCastSpell(inter.iobj[0], spellid, level, -1, spflags, duration);
		
		return Success;
	}
	
};

class PoisonCommand : public Command {
	
public:
	
	PoisonCommand() : Command("poison") { }
	
	Result execute(Context & context) {
		
		float fval = context.getFloat();
		
		DebugScript(' ' << fval);
		
		ARX_PLAYER_Poison(fval);
		
		return Success;
	}
	
};

class PlayerManaDrainCommand : public Command {
	
public:
	
	PlayerManaDrainCommand() : Command("playermanadrain") { }
	
	Result execute(Context & context) {
		
		bool enable = context.getBool();
		
		DebugScript(' ' << enable);
		
		if(enable) {
			player.playerflags &= ~PLAYERFLAGS_NO_MANA_DRAIN;
		} else {
			player.playerflags |= PLAYERFLAGS_NO_MANA_DRAIN;
		}
		
		return Success;
	}
	
};

class InvulnerabilityCommand : public Command {
	
public:
	
	InvulnerabilityCommand() : Command("invulnerability") { }
	
	Result execute(Context & context) {
		
		bool player = false;
		HandleFlags("p") {
			player = (flg & flag('p'));
		}
		
		bool enable = context.getBool();
		
		DebugScript(' ' << options << ' ' << enable);
		
		INTERACTIVE_OBJ * io = context.getIO();
		if(!player && !io) {
			ScriptWarning << "must either use -p or execute in IO context";
			return Failed;
		}
		
		
		if(enable) {
			if(player) {
				ARX_PLAYER_Invulnerability(1);
			} else {
				io->ioflags |= IO_INVULNERABILITY;
			}
		} else {
			if(player) {
				ARX_PLAYER_Invulnerability(0);
			} else {
				io->ioflags &= ~IO_INVULNERABILITY;
			}
		}
		
		return Success;
	}
	
};

}

void setupScriptedPlayer() {
	
	ScriptEvent::registerCommand(new AddBagCommand);
	ScriptEvent::registerCommand(new AddXpCommand);
	ScriptEvent::registerCommand(new AddGoldCommand);
	ScriptEvent::registerCommand(new RidiculousCommand);
	ScriptEvent::registerCommand(new RuneCommand);
	ScriptEvent::registerCommand(new QuestCommand);
	ScriptEvent::registerCommand(new SetPlayerTweakCommand);
	ScriptEvent::registerCommand(new SetHungerCommand);
	ScriptEvent::registerCommand(new SetPlayerControlsCommand);
	ScriptEvent::registerCommand(new StealNPCCommand);
	ScriptEvent::registerCommand(new SpecialFXCommand);
	ScriptEvent::registerCommand(new KeyringAddCommand);
	ScriptEvent::registerCommand(new PlayerLookAtCommand);
	ScriptEvent::registerCommand(new PrecastCommand);
	ScriptEvent::registerCommand(new PoisonCommand);
	ScriptEvent::registerCommand(new PlayerManaDrainCommand);
	ScriptEvent::registerCommand(new InvulnerabilityCommand);
	
}

} // namespace script
