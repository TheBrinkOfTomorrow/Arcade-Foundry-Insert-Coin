// Written by scripts/gen_games.py from games.toml (the app's and the cores'): edit those, not this.
#pragma once

#include <cstdint>

// How a game is played: picks the controller layout and the controls list.
enum class GameControls : uint8_t { Drive, Flight, Twin, Gun, Turret, AnalogGun, Fighting, Fishing };

struct GameListEntry {
    const char *set, *title, *detail;
    int platform; // an index into kPlatforms
    GameControls controls;
    int players;
    bool link, stick_y, gun_button, hidden;
};

// Every set the app knows, in the library's order (a card's versions in this order).
inline constexpr GameListEntry kGameList[] = {
    {"daytona93", "Daytona USA", "1993", 2, GameControls::Drive, 1, false, false, false, false},
    {"daytona", "Daytona USA", "1994, Revision A (link play)", 2, GameControls::Drive, 1, true, false, false, false},
    {"srallyc", "Sega Rally Championship", "1995, Revision C (Model 2A; link play)", 2, GameControls::Drive, 1, true, false, false, false},
    {"manxttc", "Manx TT Superbike", "1995, Revision C, Twin (Model 2A; link play)", 2, GameControls::Drive, 1, true, false, false, false},
    {"motoraid", "Motor Raid", "1997, Twin (Model 2A; West punch, South kick)", 2, GameControls::Drive, 1, false, false, false, false},
    {"doab", "Dead or Alive", "1996, Revision B (Model 2B; West punch, South kick, East hold; two players: a second gamepad)", 2, GameControls::Fighting, 2, false, false, false, false},
    {"lastbrnx", "Last Bronx", "1996, Export, Revision A (Model 2B; West punch, South kick, East guard; two players: a second gamepad)", 2, GameControls::Fighting, 2, false, false, false, false},
    {"schamp", "Sonic Championship", "1996, USA (Model 2B; West punch, South kick, East barrier; two players: a second gamepad)", 2, GameControls::Fighting, 2, false, false, false, false},
    {"dynabb97", "Dynamite Baseball 97", "1997, Revision A (Model 2B; West and South the buttons, East swings the bat; two players: a second gamepad)", 2, GameControls::Fighting, 2, false, false, false, false},
    {"waverunr", "Wave Runner", "1996, Japan, Revision A (Model 2C; the right trigger is the throttle lever)", 2, GameControls::Drive, 1, false, false, false, false},
    {"bel", "Behind Enemy Lines", "1997 (Model 2C; gun turret: the mouse or a gamepad; West the missile)", 2, GameControls::Turret, 1, false, false, false, false},
    {"topskatr", "Top Skater", "1997, Export, Revision A (Model 2C; the left stick curves and slides, South and East jump, the shoulders select)", 2, GameControls::Drive, 1, false, true, false, false},
    {"skisuprg", "Sega Ski Super G", "1996 (Model 2C; South, East and West select, the shoulders zoom, the stick swings)", 2, GameControls::Drive, 1, false, true, false, false},
    {"segawski", "Sega Water Ski", "1997, Japan, Revision A (Model 2C; Start is set, West and East the pitch buttons, the d-pad selects)", 2, GameControls::Drive, 1, false, false, false, false},
    {"desert", "Desert Tank", "1994 (right shoulder machine gun, left shoulder cannon, South/East/West VR1-3, North shift)", 2, GameControls::Drive, 1, false, false, false, false},
    {"skytargt", "Sky Target", "1995 (Model 2A; the left stick flies, South machine gun, East missile, West view change)", 2, GameControls::Flight, 1, false, false, false, false},
    {"dynamcop", "Dynamite Cop", "1998, Export (Model 2A; West punch, South kick, East jump; two players: a second gamepad)", 2, GameControls::Fighting, 2, false, false, false, false},
    {"vcop2", "Virtua Cop 2", "1995 (Model 2A; light gun: the mouse or a gamepad)", 2, GameControls::Gun, 1, false, false, false, false},
    {"vcop", "Virtua Cop", "1994, Revision B (light gun: the mouse or a gamepad)", 2, GameControls::Gun, 1, false, false, false, false},
    {"vf2", "Virtua Fighter 2", "1994, Version 2.1 (Model 2A; two players: a second gamepad)", 2, GameControls::Fighting, 2, false, false, false, false},
    {"indy500", "INDY 500", "1995, Twin, Revision A (Model 2B; link play)", 2, GameControls::Drive, 1, true, false, false, false},
    {"fvipers", "Fighting Vipers", "1995, Revision D (Model 2B; two players: a second gamepad)", 2, GameControls::Fighting, 2, false, false, false, false},
    {"sgt24h", "Super GT 24h", "1996, Jaleco (Model 2B)", 2, GameControls::Drive, 1, false, false, false, false},
    {"overrevb", "Over Rev", "1997, Jaleco (Model 2B)", 2, GameControls::Drive, 1, false, false, false, false},
    {"stcc", "Sega Touring Car Championship", "1996 (Model 2C)", 2, GameControls::Drive, 1, false, false, false, false},
    {"vstriker", "Virtua Striker", "1994, Revision A (Model 2B; West long pass, South shoot, East short pass; two players: a second gamepad)", 2, GameControls::Fighting, 2, false, false, false, false},
    {"zerogun", "Zero Gunner", "1997, Export (Model 2B; West shot, South special; two players: a second gamepad)", 2, GameControls::Fighting, 2, false, false, false, false},
    {"pltkids", "Pilot Kids", "1998, Revision A (Model 2B; West shot, South special; two players: a second gamepad)", 2, GameControls::Fighting, 2, false, false, false, false},
    {"rchase2", "Rail Chase 2", "1994, Revision A (Model 2B; gun turret: the mouse or a gamepad; two players: a second gamepad)", 2, GameControls::Turret, 2, false, false, false, false},
    {"gunblade", "Gunblade NY", "1995, Revision A (Model 2B; gun turret: the mouse or a gamepad)", 2, GameControls::Turret, 1, false, false, false, false},
    {"hotdo", "The House of the Dead", "1997 (Model 2C; light gun: the mouse or a gamepad)", 2, GameControls::Gun, 1, false, false, false, false},
    {"von", "Cyber Troopers Virtual-On", "1995, Twin, Export (Model 2B; twin sticks: both sticks of the gamepad)", 2, GameControls::Twin, 1, false, false, false, false},
    {"vr", "Virtua Racing", "1992 (link play)", 1, GameControls::Drive, 1, true, false, false, false},
    {"swa", "Star Wars Arcade", "1993 (the left stick flies)", 1, GameControls::Flight, 1, false, false, false, false},
    {"vf", "Virtua Fighter", "1993 (two players: a second gamepad)", 1, GameControls::Fighting, 2, false, false, false, false},
    {"wingwar", "Wing War", "1994 (the left stick flies)", 1, GameControls::Flight, 1, false, false, false, false},
    {"vf3", "Virtua Fighter 3", "1996 (two players: a second gamepad)", 3, GameControls::Fighting, 2, false, false, false, false},
    {"scud", "Scud Race", "1996", 3, GameControls::Drive, 1, false, false, false, false},
    {"scudplus", "Scud Race", "1997, Scud Race Plus", 3, GameControls::Drive, 1, false, false, false, false},
    {"daytona2", "Daytona USA 2", "1998, Battle on the Edge", 3, GameControls::Drive, 1, false, false, false, false},
    {"dayto2pe", "Daytona USA 2", "1998, Power Edition", 3, GameControls::Drive, 1, false, false, false, false},
    {"srally2", "Sega Rally 2", "1998 (South view change, East hand brake)", 3, GameControls::Drive, 1, false, false, false, false},
    {"lemans24", "Le Mans 24", "1997", 3, GameControls::Drive, 1, false, false, false, false},
    {"dirtdvls", "Dirt Devils", "1998 (South view change)", 3, GameControls::Drive, 1, false, false, false, false},
    {"harley", "Harley-Davidson & L.A. Riders", "1997 (South view change, East music select; two coins per credit)", 3, GameControls::Drive, 1, false, false, false, false},
    {"ecau", "Emergency Call Ambulance", "1999 (South view change)", 3, GameControls::Drive, 1, false, false, false, false},
    {"fvipers2", "Fighting Vipers 2", "1998 (West punch, South kick, East guard; two players: a second gamepad)", 3, GameControls::Fighting, 2, false, false, false, false},
    {"von2", "Cyber Troopers Virtual-On Oratorio Tangram", "1998 (twin sticks: both sticks of the gamepad; two coins per credit)", 3, GameControls::Twin, 1, false, false, false, false},
    {"spikeout", "Spikeout", "1998 (West beat, South charge, East shift, North jump)", 3, GameControls::Fighting, 1, false, false, false, false},
    {"spikeofe", "Spikeout", "1999, Final Edition (West beat, South charge, East shift, North jump)", 3, GameControls::Fighting, 1, false, false, false, false},
    {"vs299a", "Virtua Striker 2 '99", "1999 (West short pass, South long pass, East shoot; two players: a second gamepad)", 3, GameControls::Fighting, 2, false, false, false, false},
    {"swtrilgy", "Star Wars Trilogy Arcade", "1998 (the left stick flies, left shoulder second trigger, East/West event buttons)", 3, GameControls::Flight, 1, false, false, false, false},
    {"lostwsga", "The Lost World", "1997 (light gun: the mouse or a gamepad; right button reloads)", 3, GameControls::AnalogGun, 1, false, false, false, false},
    {"lamachin", "L.A. Machineguns", "1998 (light gun: the mouse or a gamepad)", 3, GameControls::AnalogGun, 1, false, false, false, false},
    {"oceanhuna", "The Ocean Hunter", "1998 (light gun: the mouse or a gamepad; two coins per credit)", 3, GameControls::AnalogGun, 1, false, false, false, false},
    {"bassdx", "Sega Bass Fishing", "1998, Deluxe, USA (the left stick is the rod, the right trigger reels, the d-pad the lure stick, West casts, South selects)", 3, GameControls::Fishing, 1, false, true, false, false},
    {"getbass", "Get Bass", "1998, Japan, Standard (the left stick is the rod, the right trigger reels, West casts)", 3, GameControls::Fishing, 1, false, true, false, false},
    {"getbassur", "Get Bass", "1998, Japan, Upright (the left stick is the rod, the right trigger reels, West casts)", 3, GameControls::Fishing, 1, false, true, false, false},
    {"getbassdx", "Get Bass", "1998, Japan, Deluxe (needs bassdx.zip beside it; the left stick is the rod, the right trigger reels, West casts; two coins per credit)", 3, GameControls::Fishing, 1, false, true, false, false},
    {"magtruck", "Magical Truck Adventure", "1998, Japan (pump the lever: the stick up and down; West the pedal; two players: a second gamepad)", 3, GameControls::Fighting, 2, false, false, false, false},
    {"hotd4a", "The House of the Dead 4", "2005, Revision A (light gun: the mouse or a gamepad; middle-click, G or West throws a grenade)", 4, GameControls::Gun, 1, false, false, true, false},
    {"letsgoju", "Let's Go Jungle!", "2006, Lost on the Island of Spice (light gun: the mouse or a gamepad; hold to fire)", 4, GameControls::Gun, 1, false, false, false, false},
    {"segartv", "Sega Race TV", "2008 (wheel and pedals)", 4, GameControls::Drive, 1, false, false, false, false},
    {"outr2sdx", "OutRun 2 SP SDX", "2006 (wheel and pedals)", 4, GameControls::Drive, 1, false, false, false, false},
    {"outrun", "Out Run", "1986, Revision B (Out Run board; the shoulders move the gear lever: low and high)", 0, GameControls::Drive, 1, false, false, false, false},
    {"shangon", "Super Hang-On", "1987, Sit-down/upright (Out Run board; the right shoulder or South is the turbo)", 0, GameControls::Drive, 1, false, false, false, false},
    {"aburner131", "After Burner", "1987, VER 1.31, deluxe/upright (X Board; the left stick flies; South or the right shoulder fires the vulcan, East the missiles)", 0, GameControls::Flight, 1, false, false, false, false},
    {"aburner2", "After Burner II", "1987 (X Board; the left stick flies, the right trigger is the throttle; South or the right shoulder fires the vulcan, East the missiles)", 0, GameControls::Flight, 1, false, false, false, false},
    {"rachero", "Racing Hero", "1989 (X Board; lean with the left stick; the right trigger is the throttle, the left the brake)", 0, GameControls::Drive, 1, false, false, false, false},
    {"loffire", "Line of Fire", "1989, World (X Board; light gun: the mouse or a gamepad; two gun turrets; middle-click, G or West fires a missile; two players: a second gamepad)", 0, GameControls::Gun, 2, false, false, true, false},
    {"gprider", "GP Rider", "1990, World (X Board; lean with the left stick; the right trigger is the throttle, the left the brake, the shoulders shift up and down)", 0, GameControls::Drive, 1, false, false, false, false},
    {"smgp", "Super Monaco GP", "1989, World, upright (X Board; steer with the left stick; the right trigger accelerates, the left brakes, the shoulders shift up and down)", 0, GameControls::Drive, 1, false, false, false, false},
    {"thndrbld", "Thunder Blade", "1987, Upright (X Board; the left stick flies, the right trigger is the throttle; South or the right shoulder fires the guns, East the missiles)", 0, GameControls::Flight, 1, false, false, false, false},
    {"toutrun", "Turbo Out Run", "1989, Out Run upgrade (Out Run board; the shoulders move the gear lever: low and high; South is the turbo)", 0, GameControls::Drive, 1, false, false, false, false},
    {"orunners", "OutRunners", "1992, World (System 32; the shoulders shift; South, East and West choose the music)", 0, GameControls::Drive, 1, false, false, false, false},
    {"hangon", "Hang-On", "1985, Upright/ride-on (Hang-On board; lean with the left stick; the right trigger is the throttle, the left the brake)", 0, GameControls::Drive, 1, false, false, false, false},
    {"sharrier", "Space Harrier", "1985, Rev A, upright/sit-down (Hang-On board; the left stick flies; South or the right shoulder fires)", 0, GameControls::Flight, 1, false, false, false, false},
    {"enduror", "Enduro Racer", "1986, Rev A, YM2151 (Hang-On board; steer with the left stick and pull it back for a wheelie; the right trigger is the throttle, the left the brake)", 0, GameControls::Drive, 1, false, true, false, false},
    {"radr", "Rad Rally", "1991, World (System 32; the shoulders move the gear lever: low and high)", 0, GameControls::Drive, 1, false, false, false, false},
    {"radm", "Rad Mobile", "1990, World (System 32; South the lights, East the wiper)", 0, GameControls::Drive, 1, false, false, false, false},
    {"f1en", "F1 Exhaust Note", "1991, World, Rev A (System 32; steer to pick automatic or manual and press the gas; with manual the shoulders shift up and down)", 0, GameControls::Drive, 1, false, false, false, false},
    {"jpark", "Jurassic Park", "1993, World, Rev A (System 32; gun turret: the mouse or a gamepad; the game draws each gun's sight; two players: a second gamepad)", 0, GameControls::Turret, 2, false, false, false, false},
    {"sonic", "SegaSonic the Hedgehog", "1992, Japan, Rev C (System 32; the stick or d-pad rolls the trackball; West or South spin jumps; two players: a second gamepad)", 0, GameControls::Fighting, 2, false, false, false, false},
    {"ga2", "Golden Axe: The Revenge of Death Adder", "1992, World, Rev B (System 32; West South East the three buttons; East casts magic; two players: a second gamepad)", 0, GameControls::Fighting, 2, false, false, false, false},
    {"gforce2", "Galaxy Force 2", "1988, World, upright (Y Board; the left stick flies, the right trigger is the throttle; South or the right shoulder fires, East the missiles)", 0, GameControls::Flight, 1, false, true, false, false},
    {"pdrift", "Power Drift", "1988, World, Rev A (Y Board; steer with the left stick; the right trigger accelerates, the left brakes; the shoulders move the gear lever: low and high)", 0, GameControls::Drive, 1, false, false, false, false},
    {"gloc", "G-LOC Air Battle", "1990, World (Y Board; the left stick flies, the right trigger is the throttle; South or the right shoulder fires the vulcan, East the missiles, West the after burner)", 0, GameControls::Flight, 1, false, true, false, false},
    {"strkfgtr", "Strike Fighter", "1991, World (Y Board; the left stick flies, the right trigger is the throttle; South or the right shoulder fires the vulcan, East the missiles, West the after burner)", 0, GameControls::Flight, 1, false, true, false, false},
    {"rchase", "Rail Chase", "1991, World (Y Board; gun turret: the mouse or a gamepad; two guns on the rail cars; the game draws each gun's sight; two players: a second gamepad)", 0, GameControls::Turret, 2, false, false, false, false},
    {"hotd3", "The House of the Dead III", "2002 (light gun: the mouse or a gamepad; two players: a second gamepad)", 5, GameControls::Gun, 2, false, false, false, true},
    {"vcop3", "Virtua Cop 3", "2003 (light gun: the mouse or a gamepad; middle-click, G or West changes weapon; two players: a second gamepad)", 5, GameControls::Gun, 2, false, false, true, true},
    {"crtaxihr", "Crazy Taxi High Roller", "2003 (steer with the left stick; the right trigger accelerates, the left brakes; the right shoulder is Drive, the left Reverse)", 5, GameControls::Drive, 1, false, false, false, true},
};

// Other names a set's files may have (file name, set): MAME's parents and older
// dumps that hold it, and the app's earlier names for it.
inline constexpr const char *kOtherNames[][2] = {
    {"doa", "doab"},
    {"hotd", "hotdo"},
    {"hotd4", "hotd4a"},
    {"lgj", "letsgoju"},
    {"or2sp", "outr2sdx"},
};

// The app's earlier names for a set (old, new): saves and settings move once.
inline constexpr const char *kRenamedSets[][2] = {
    {"hotd4", "hotd4a"},
    {"lgj", "letsgoju"},
    {"or2sp", "outr2sdx"},
};

// A Chihiro game's XBE, in its folder (set, file).
inline constexpr const char *kChihiroXbe[][2] = {
    {"hotd3", "hod3xb.xbe"},
    {"vcop3", "vc3.xbe"},
    {"crtaxihr", "ctx_ac[r].xbe"},
};
