/*
 * $Header: /home/ozzie/Download/0000/uhexen2/gamecode/hc/h2/soul.hc,v 1.3 2005-02-23 08:13:25 sezero Exp $
 */

// Possible improvement: Make model shrink before disappearing

void () crusader_soul_touch =
{
	if(!other.flags2&FL_ALIVE)
		return;

	if ((other.classname == "player") && (other.playerclass==CLASS_CRUSADER))
	{
		sound (self, CHAN_VOICE, "items/artpkup.wav", 1, ATTN_NORM);
		other.super_damage_time = time + 30;
		other.super_damage = 1;
		other.super_damage_low = 0;
		// Pa3PyX: since holy strength now only spawns starting at
		//         clvl 6 now (as opposed to 4), we offset this negative
		//         change by adding small health bonus to it (+clvl)
		if (other.health < other.max_health) {
			other.health += other.level;
			if (other.health > other.max_health) {
				other.health = other.max_health;
			}
		}
		// Pa3PyX: end code

		self.touch = SUB_Null;
		self.think=SUB_Remove;
		thinktime self : HX_FRAME_TIME * 2;

		self.enemy.think=SUB_Remove;
		thinktime self.enemy : HX_FRAME_TIME * 2;

		sprint (other, "You now have Holy Strength!\n");
	}
	// Bad people are hurt by this
	else if ((other.classname == "player") && 
		((other.playerclass==CLASS_NECROMANCER) || (other.playerclass==CLASS_ASSASSIN)))
	{
		if (self.pain_finished < time)
			T_Damage (other, self, self, 5);
		self.pain_finished = time + .2;
	}
};

void () necro_soul_touch =
{
	float pot_mult;	// Pa3PyX

	if(!other.flags2&FL_ALIVE)
		return;

	if ((other.classname == "player") && (other.playerclass==CLASS_NECROMANCER))
	{
		sound (self, CHAN_VOICE, "items/artpkup.wav", 1, ATTN_NORM);
	/*	other.health += self.health;
		if (other.health>other.max_health)
			other.health = other.max_health;
		
		other.bluemana += self.bluemana;
		if (other.bluemana > other.max_mana)
			other.bluemana = other.max_mana;
		
		other.greenmana += self.greenmana;
		if (other.greenmana > other.max_mana)
			other.greenmana = other.max_mana;
	*/ /*	Pa3PyX: We make the maximum health at 30,
			just as it was, but:
		1) Mana bonuses were not working. self(sphere).__mana
		   fields are not set anywhere. Now the Necromancer
		   should also gain mana from soul spheres.
		2) Bonuses will wane as time goes, to 0 at 15 secs.
		   They must be picked up immediately for full effect.
		   "Soul Spheres quickly lose their potency, so the
		    Necromancer must be swift!"
		3) The health bonus will no longer cancel the effect
		   of the Mystic Urn (above max hitpoints).	*/
		pot_mult = 15.0 - time + self.lifetime;
		if (pot_mult < 0) {
			pot_mult = 0;
		}
		if (other.health < other.max_health) {
			sprint(other, ftos(self.lifetime));
			other.health += 2.0 * pot_mult;
			if (other.health > other.max_health) {
				other.health = other.max_health;
			}
		}
		other.bluemana += pot_mult;
		if (other.bluemana > other.max_mana) {
			other.bluemana = other.max_mana;
		}
		other.greenmana += pot_mult;
		if (other.greenmana > other.max_mana) {
			other.greenmana = other.max_mana;
		}

		sprint (other, "You have devoured a life force!\n");

		self.touch = SUB_Null;
		self.think=SUB_Remove;
		thinktime self : HX_FRAME_TIME * 2;

		self.enemy.think=SUB_Remove;
		thinktime self.enemy : HX_FRAME_TIME * 2;

	}
	// Good people are hurt by this
	else if ((other.classname == "player") && 
		((other.playerclass==CLASS_PALADIN) || (other.playerclass==CLASS_CRUSADER)))
	{
		if (self.pain_finished < time)
			T_Damage (other, self, self, 5);
		self.pain_finished = time + .2;

	}
};

void () soul_move =
{
	vector vel;
	float p_color;

	self.velocity_z += self.hoverz;
	if (self.velocity_z > 16)
		self.hoverz = -1;		
	else if (self.velocity_z < -16 )
		self.hoverz = 1;

	if (self.classname == "soulskull") 
	{
		p_color = 144;
		vel = randomv('-1.44 -1.44 -65', '1.44 1.44 -35');
		particle2(self.origin - '0 0 20',vel,vel,p_color,PARTICLETYPE_C_EXPLODE,self.health/4);
	}

	else if (self.classname == "soulcross")
	{	
		p_color = 176;
		vel = randomv('-1.44 -1.44 65', '1.44 1.44 35');
		particle2(self.origin +'0 0 25',vel,vel,p_color,PARTICLETYPE_C_EXPLODE,self.health/4);
	}

	setorigin (self.enemy, self.origin);	// Move the ball with it

	self.health -= 0.1;
	
	self.think = soul_move;
	thinktime self : 0.1;
	if (self.sound_time < time)
	{
		self.sound_time = time + 4;
		sound (self, CHAN_VOICE, "raven/soul.wav", 1, ATTN_NORM);
	}	

	if (self.health <= 0)
	{
		sound (self, CHAN_VOICE, "misc/null.wav", 1, ATTN_NORM);
		remove(self);
		remove(self.enemy);
	}
};


void necromancer_sphere (entity ent)
{
	entity new,new2;
	float chance;

//	chance = .05 + ((ent.level - 3) * .03);
	// Pa3PyX: adjusted chances (this artifact is powerful, and our sickle
	//	   leech has been improved, so we nerf this a bit. Start at
	//	   2.5% chance at clvl 3, increase to 20% at clvl 10
	chance = (ent.level - 2) * 0.025;
	if (chance > .2)
		chance = .2;

	if (random() > chance)
		return;

	new2 = spawn();

	new2.owner = new;
	new2.solid = SOLID_TRIGGER;
	new2.movetype = MOVETYPE_FLY;

	droptofloor();
	setorigin (new2, self.origin + '0  0 32');
	setmodel (new2, "models/soulskul.mdl");
	new2.classname = "soulskull";
	setsize (new2, new2.mins , new2.maxs);
	new2.think = soul_move;
	thinktime new2 : 0.1;
	new2.hoverz=1;
	new2.velocity_z=new.hoverz;
	new2.touch = necro_soul_touch;
	new2.flags=0;
	new2.lifespan = 15;  // Alive for 15 seconds
	new2.health = new2.lifespan * 2;
	// Pa3PyX: remember for how long the sphere was around
	new2.lifetime = time;

	new2.avelocity_y = 200;
	sound (new, CHAN_VOICE,"raven/soul.wav", 1, ATTN_NORM);


	new = spawn();
	new.owner = new;
	new.solid = SOLID_TRIGGER;
	new.movetype = MOVETYPE_FLY;

	setmodel (new, "models/soulball.mdl");

	setsize (new, new.mins , new.maxs);
	new.flags=0;
	setorigin (new, new2.origin);

	new2.enemy = new;

}

void crusader_sphere (entity ent)
{
	local entity new,new2;
	float chance;

//	chance = .05 + ((ent.level - 3) * .03);
	// Pa3PyX: adjusted to start at level 6, reach 0.2 at level 10
	chance = .04 + ((ent.level - 6) * 0.04);
	if (chance > .2)
		chance = .2;

	if (random() > chance)
		return;

	new2 = spawn();

	new2.owner = new2;
	new2.solid = SOLID_TRIGGER;
	new2.movetype = MOVETYPE_FLY;

	droptofloor();
	setorigin (new2, self.origin + '0  0 32');
	setmodel (new2, "models/cross.mdl");

	setsize (new2, new2.mins , new2.maxs);
	new2.classname = "soulcross";
	new2.touch = crusader_soul_touch;
	new2.think = soul_move;
	thinktime new2 : 0.1;
	new2.hoverz=1;
	new2.velocity_z=new.hoverz;
	new2.flags=0;
	new2.lifespan = 15;  // Alive for 15 seconds
	new2.health = new2.lifespan * 2;

	new2.avelocity_y = 200;
	sound (new, CHAN_VOICE, "raven/soul.wav", 1, ATTN_NORM);


	new = spawn();

	new.owner = new;
	new.solid = SOLID_TRIGGER;
	new.movetype = MOVETYPE_FLY;

	setorigin (new, new2.origin);
	setmodel (new, "models/goodsphr.mdl");

	new.drawflags (+) MLS_ABSLIGHT;

	setsize (new, new.mins , new.maxs);
	new.hoverz=1;
	new.velocity_z=new.hoverz;
	new.flags=0;
	new.lifespan = 15;  // Alive for 15 seconds
	new.health = new.lifespan * 2;
	new.sound_time = 2;

	new2.enemy = new;
}

/*
 * $Log: not supported by cvs2svn $
 * Revision 1.2  2005/02/23 08:12:24  sezero
 * crusader holy strength starts at level 6 (from Pa3PyX)
 *
 * Revision 1.1.1.1  2004/11/29 11:40:40  sezero
 * Initial import
 *
 * 
 * 27    9/02/97 9:20p Rlove
 * 
 * 26    9/01/97 12:18a Rlove
 * 
 * 25    9/01/97 12:13a Rlove
 * 
 * 24    8/30/97 2:29p Jweier
 * 
 * 23    8/29/97 1:00p Mgummelt
 * Spheres were floating too high
 * 
 * 22    8/07/97 5:45p Rlove
 * 
 * 21    8/06/97 10:14p Mgummelt
 * 
 * 20    8/06/97 11:12a Rlove
 * 
 * 19    8/04/97 12:33p Rlove
 * 
 * 18    8/01/97 4:48p Rlove
 * 
 * 17    8/01/97 4:38p Rlove
 * 
 * 16    7/21/97 3:03p Rlove
 * 
 * 15    7/01/97 11:23a Rlove
 * 
 * 14    7/01/97 11:06a Rlove
 * 
 * 13    7/01/97 9:46a Rlove
 * Crusader soul sphere is in. It does double damage.
 * 
 * 12    6/30/97 7:30p Rlove
 * 
 * 11    6/19/97 4:11p Rjohnson
 * Optimization
 * 
 * 10    6/19/97 4:08p Rjohnson
 * removed crandom()
 * 
 * 9     6/18/97 10:46a Rjohnson
 * Code cleanu
 * 
 * 8     6/17/97 10:20a Rlove
 * 
 * 7     6/16/97 8:04a Rlove
 * Fixed null.wav error, also paladin weapons deplete mana now
 * 
 * 6     6/03/97 10:48p Mgummelt
 * 
 * 5     6/03/97 7:59a Rlove
 * Change take_art.wav to artpkup.wav
 * 
 * 4     5/15/97 6:34p Rjohnson
 * Code cleanup
 * 
 * 3     3/12/97 10:57p Rjohnson
 * Changed the particle2 parameters to make it less taxing on the network
 * and more versitile
 * 
 * 2     2/03/97 4:42p Rlove
 * Newest soul sphere
 * 
 * 1     2/03/97 2:43p Rlove
 */
