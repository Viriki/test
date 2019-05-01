#include "ScriptMgr.h"
#include "tomb_of_sargeras.h"
#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "SpellAuras.h"
#include "ScriptedCreature.h"
#include "InstanceScript.h"

enum Spells
{
    SPELL_ABRASIVE_ARMOR          = 233071, // Works.

    SPELL_UNCHECKED_RAGE          = 231854, // Works but still didn't test the 5+ targets.
    SPELL_UNCHECKED_RAGE_SINGLE   = 247403, // Used on the boss' target.
    SPELL_ENRAGED                 = 241587, // Applies to every friendly unit nearby I think.

    SPELL_COMMANDING_ROAR         = 232192,
    SPELL_COMMANDING_ROAR_TARGET1 = 239425,
    SPELL_COMMANDING_ROAR_TARGET2 = 239426,
    SPELL_COMMANDING_ROAR_TARGET3 = 239427,

    SPELL_DRAW_IN                 = 232061, // Works.

    SPELL_FRIGID_BLOWS            = 233548,
    SPELL_FRIGID_BLOWS_AURA       = 233429, // Its using all of the stacks with one melee attack. Also triggers 233548 which is used for Drenching Slough.
    SPELL_FRIGID_BLOWS_TRIGGER    = 233553, // add a trigger in spellmgr

    SPELL_DRENCHING_SLOUGH        = 233526,
    SPELL_DRENCHING_SLOUGH_AT     = 233530,

    SPELL_FROSTY_DISCHARGE        = 232174, // Works.
    SPELL_CANCEL_DRENCHED_AURA    = 239926, // Works.

    SPELL_DRENCHED                = 231770, // Not sure but the periodic is getting reset every time reapplied. It also dissapears when the player is out of combat.

    // Razorjaw Wavemender
    SPELL_WATERY_SPLASH           = 233371, // Missile. Should be interruptable.

    SPELL_AQUEOUS_BURST           = 240725,
    SPELL_AQUEOUS_BURST_AURA      = 231729, // cancels the aura before it finishes preventing the at to trigger
    SPELL_AQUEOUS_BURST_AT        = 231754, // Triggered after the aura. Deals a burst damage and leaves a AT. Fix the radius
    SPELL_AQUEOUS_BURST_IMMUNITY  = 240726, // 10s Aura to prevent the same player from getting an other Aqueous Burst.
    SPELL_DRENCHING_WATERS        = 231768, // Aqueous Burst AT's damage. Applies Drenched.

    // Razorjaw Gladiator
    SPELL_DRIPPING_BLADE          = 239907, // Applies Drenched on melee attacks with a 4s CD.

    SPELL_DRIVEN_ASSAULT          = 240789,
    SPELL_DRIVEN_ASSAULT_FIXATE   = 234016, 
    SPELL_DRIVEN_ASSAULT_BUFF     = 234128, // Doesn't increase damage nor reduces movement speed.
    SPELL_DRIVEN_ASSAULT_IMMUNITY = 240792, // 10s aura to prevent the same player from getting targeted again.

    SPELL_SPLASHY_CLEAVE          = 234129, // Cleaves the target doing dmg and applying drenched in a cone infront of the caster. Seems to be working though didn't try on 1+ targets.
};

Position const SpawnPos[5] =
{
    { 5954.609863f, -923.536011f, 2936.709961f, 4.166270f },
    { 5951.979980f, -902.309021f, 2935.489990f, 3.075160f },
    { 5902.189941f, -875.427979f, 2941.080078f, 5.843960f },
    { 5876.870117f, -886.231018f, 2932.659912f, 0.792863f },
    { 5869.359863f, -903.413025f, 2938.219971f, 5.904540f }
};

Position const JumpPos[5] =
{
    { 5942.613770f, -920.371826f, 2919.454590f, 2.996035f },
    { 5941.242676f, -903.601013f, 2919.412109f, 3.644811f },
    { 5908.149902f, -881.304810f, 2919.416748f, 5.148858f },
    { 5889.001465f, -892.704590f, 2919.862549f, 5.655442f },
    { 5880.822266f, -905.127930f, 2919.772217f, 6.244478f }
};

// 116407 - Harjatan <The Bludgeoner>
struct boss_harjatan : public BossAI
{
    boss_harjatan(Creature* creature) : BossAI(creature, DATA_HARJATAN)
    {
        me->SetReactState(REACT_PASSIVE);
        me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_IMMUNE_TO_NPC | UNIT_FLAG_IMMUNE_TO_PC);
        me->SetPowerType(POWER_ENERGY);
        me->SetMaxPower(POWER_ENERGY, 100);

        _DidSayLOS1 = false;
        _DidSayFirstPackDead = false;
        _DidStartIntro = true;
        _IsIntroEnded = true;
        _Instance = me->GetInstanceScript();
    }

    enum Events
    {
        EVENT_UNCHECKED_RAGE = 1,
        EVENT_COMMANDING_ROAR,
        EVENT_DRAW_IN,
        EVENT_POWER_REGEN
    };

    enum Talks
    {
        SAY_AGGRO,
        SAY_DIED,
        SAY_KILLED_PLAYER,
        SAY_COMMANDING_ROAR,
        SAY_INTRO2
    };

    enum MistressSassizineTalks
    {
        SAY_LOS1,
        SAY_LOS2,
        SAY_FIRST_PACK_DIES,
        SAY_INTRO1,
        SAY_INTRO3 = 5,
        SAY_OUTRO, // Different npc (115767)
    };

    enum Misc
    {
        END_INTRO = 10,
    };

    void EnterCombat(Unit* /*who*/) override
    {
        _EnterCombat();

        Talk(SAY_AGGRO);
    }

    void Reset() override
    {
        _Reset();

        me->SetPower(POWER_ENERGY, 0);

        CreatureList creatureList;
        me->GetCreatureListWithEntryInGrid(creatureList, NPC_RAZORJAW_GLADIATOR);
        me->GetCreatureListWithEntryInGridAppend(creatureList, NPC_RAZORJAW_WAVEMENDER);

        for (Creature* creature : creatureList)
        {
            if (creature->IsSummon())
                creature->DespawnOrUnsummon(1);
        }

        me->RemoveAreaTrigger(SPELL_AQUEOUS_BURST_AT);

        me->RemoveAurasDueToSpell(SPELL_ENRAGED);
        me->RemoveAurasDueToSpell(SPELL_FRIGID_BLOWS_AURA);

        if (_Instance != nullptr)
        {
            _Instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_DRENCHED);
            _Instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_AQUEOUS_BURST_AURA);
            _Instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_DRIVEN_ASSAULT_FIXATE);
        }

        if (!me->HasAura(SPELL_ABRASIVE_ARMOR))
            DoCastSelf(SPELL_ABRASIVE_ARMOR, true);

        _EnergyFilledCounter = 0;
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        _EnterEvadeMode();

        me->InterruptNonMeleeSpells(true);

        me->ClearUnitState(UnitState::UNIT_STATE_ROOT);
        me->ClearUnitState(UnitState::UNIT_STATE_DISTRACTED);
        me->ClearUnitState(UnitState::UNIT_STATE_STUNNED);

        me->SetReactState(ReactStates::REACT_AGGRESSIVE);

        if (_Instance != nullptr)
        {
            me->RemoveAurasDueToSpell(SPELL_ENRAGED);
            me->RemoveAurasDueToSpell(SPELL_FRIGID_BLOWS_AURA);

            _Instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_DRENCHED);
            _Instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_AQUEOUS_BURST_AURA);
            _Instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_DRIVEN_ASSAULT_FIXATE);

            _Instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);

            _Instance->SetBossState(DATA_HARJATAN, FAIL);
        }
    }

    void MoveInLineOfSight(Unit* who) override
    {
        if (who->IsPlayer() && who->GetDistance(me) <= 50.f && _DidSayLOS1 == false)
        {
            CreatureList List;
            me->GetCreatureListWithEntryInGrid(List, NPC_MISTRESS_SASSZINE, 20.f);
            _Mistress = List.front();

            _Mistress->AI()->Talk(SAY_LOS1);
            _Intro.ScheduleEvent(SAY_LOS2, 10s);
            _IsIntroEnded = false;
            _DidSayLOS1 = true;
        }
    }

    /*void JustSummoned(Creature* summon) override
    {
        uint8 PosId = 0;
        if ((summon->GetEntry() == NPC_RAZORJAW_GLADIATOR || summon->GetEntry() == NPC_RAZORJAW_WAVEMENDER) && (summon->IsSummon()))
        {
            for (uint8 Itr = 0; Itr < 5; Itr++) // We need to verify the spawn position of the creature.
            {
                if (me->GetDistance(SpawnPos[Itr]) <= 5.f) // 5 Yards just in case.
                {
                    PosId = Itr;
                    break;
                }
            }

            me->GetMotionMaster()->MoveJump(JumpPos[PosId], 10.f, 10.f, 1004U, true);
        }
    }*/

    void ScheduleTasks() override
    {
        events.ScheduleEvent(EVENT_COMMANDING_ROAR, 17200);
        events.ScheduleEvent(EVENT_POWER_REGEN, 1s);
    }

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
        case EVENT_UNCHECKED_RAGE:
        {
            me->CastSpell(me->GetVictim(), SPELL_UNCHECKED_RAGE);
            break;
        }
        case EVENT_COMMANDING_ROAR:
        {
            me->CastSpell(nullptr, SPELL_COMMANDING_ROAR);
            Talk(SAY_COMMANDING_ROAR);

            events.Repeat(30s);
            break;
        }
        case EVENT_DRAW_IN:
        {
            me->CastSpell(nullptr, SPELL_DRAW_IN);
            break;
        }
        case EVENT_POWER_REGEN:
        {
            me->ModifyPower(POWER_ENERGY, 5);

            if(me->GetPower(POWER_ENERGY) == 100)
                if (_EnergyFilledCounter < 3)
                {
                    events.ScheduleEvent(EVENT_UNCHECKED_RAGE, 1);
                    _EnergyFilledCounter++;
                }
                else
                {
                    events.ScheduleEvent(EVENT_DRAW_IN, 1);
                    _EnergyFilledCounter = 0;
                }

            events.Repeat(1s);
            break;
        }
        }
    }

    void UpdateAI(uint32 diff) override
    {
        // Intro
        if (!_IsIntroEnded)
        {
            _Intro.Update(diff);

            if(uint32 eventId = _Intro.ExecuteEvent())
                switch (eventId)
                {
                case SAY_LOS2:
                {
                    _Mistress->AI()->Talk(SAY_LOS2);
                    _DidStartIntro = false;
                    break;
                }
                case SAY_INTRO1:
                {
                    _Mistress->AI()->Talk(SAY_INTRO1);
                    _Intro.ScheduleEvent(SAY_INTRO2, 5s);
                    break;
                }
                case SAY_INTRO2:
                {
                    Talk(SAY_INTRO2);
                    _Intro.ScheduleEvent(SAY_INTRO3, 4s);
                    break;
                }
                case SAY_INTRO3:
                {
                    _Mistress->AI()->Talk(SAY_LOS2);
                    _Intro.ScheduleEvent(END_INTRO, 1s);
                    break;
                }
                case END_INTRO:
                {
                    me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_IMMUNE_TO_NPC | UNIT_FLAG_IMMUNE_TO_PC);
                    me->SetReactState(REACT_AGGRESSIVE);

                    _Mistress->DespawnOrUnsummon(1);

                    _IsIntroEnded = true;

                    break;
                }
                case SAY_OUTRO:
                {
                    CreatureList BossMistress; 
                    me->GetCreatureListWithEntryInGrid(BossMistress, NPC_SASSZINE, 2000.f); // the actual boss
                    if (BossMistress.empty())
                        break;

                    BossMistress.front()->AI()->Talk(SAY_OUTRO);
                    break;
                }
                }
        }

        if (!_DidStartIntro)
        {
            CreatureList SmallAddsList;
            CreatureList BigAddList;
            bool IsFirstPackDead = true;
            bool IsSecondPackDead = true;

            me->GetCreatureListWithEntryInGrid(SmallAddsList, NPC_RAZORJAW_ACOLYTE, 40.f); // Distance is probably less.
            me->GetCreatureListWithEntryInGrid(BigAddList, NPC_TIDESCALE_WITCH, 40.f);

            IsFirstPackDead = IsPackDead(SmallAddsList, BigAddList);

            if (IsFirstPackDead && !_DidSayFirstPackDead)
            {
                _Mistress->AI()->Talk(SAY_FIRST_PACK_DIES);
                _DidSayFirstPackDead = true;
            }

            // We want to see if the second pack is dead or not to schedule the intro.
            me->GetCreatureListWithEntryInGrid(SmallAddsList, NPC_RAZORJAW_MYRMIDON, 40.f); // Distance is probably less.
            me->GetCreatureListWithEntryInGrid(BigAddList, NPC_TIDESCALE_LEGIONAIRE, 40.f);

            IsSecondPackDead = IsPackDead(SmallAddsList, BigAddList);

            if (IsSecondPackDead && !_DidSayFirstPackDead)
            {
                _Mistress->AI()->Talk(SAY_FIRST_PACK_DIES);
                _DidSayFirstPackDead = true;
            }

            if (IsSecondPackDead && IsFirstPackDead)
            {
                _Intro.ScheduleEvent(SAY_INTRO1, 1s);
                _DidStartIntro = true;
            }
        }
        // End of Intro

        if (!UpdateVictim())
            return;

        events.Update(diff);

        if (uint32 eventId = events.ExecuteEvent())
        {
            if (me->HasUnitState(UNIT_STATE_CASTING))
                events.RescheduleEvent(eventId, 10ms);
            else
                ExecuteEvent(eventId);
        }

        DoMeleeAttackIfReady();
    }

    void JustDied(Unit* /*Who*/) override
    {
        _JustDied();

        Talk(SAY_DIED);
        _Intro.ScheduleEvent(SAY_OUTRO, 10s);

        me->DespawnCreaturesInArea(NPC_RAZORJAW_GLADIATOR); // We want to despawn all of them.
        me->DespawnCreaturesInArea(NPC_RAZORJAW_WAVEMENDER);

        me->RemoveAreaTrigger(SPELL_AQUEOUS_BURST_AT);

        if (_Instance != nullptr)
        {
            _Instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);

            _Instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_DRENCHED);
            _Instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_AQUEOUS_BURST_AURA);
            _Instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_DRIVEN_ASSAULT_FIXATE);

            _Instance->SetBossState(DATA_HARJATAN, DONE);
        }
    }

    void KilledUnit(Unit* victim) override
    {
        _KilledUnit(victim);

        if(victim->IsPlayer())
            Talk(SAY_KILLED_PLAYER);
    }

    bool IsPackDead(CreatureList SmallAddsList, CreatureList BigAddList)
    {
        if (SmallAddsList.empty() && BigAddList.empty())
            return true;

        if (BigAddList.front()->isDead())
        {
            for (Creature* creature : SmallAddsList)
            {
                if (creature->IsAlive())
                {
                    return false;
                }
            }
            return true;
        }
        return false;
    }
private:
    EventMap _Intro;
    bool _DidSayLOS1;
    bool _DidSayFirstPackDead;
    bool _DidStartIntro;
    bool _IsIntroEnded;
    Creature* _Mistress;
    uint16 _EnergyFilledCounter;
    InstanceScript* _Instance;
};

// 117596 - Razorjaw Gladiator
struct npc_razor_jaw_gladiator : public ScriptedAI
{
    npc_razor_jaw_gladiator(Creature* creature) : ScriptedAI(creature)
    {
        me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_TAUNT, true);
        me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_ATTACK_ME, true);


        if (me->IsSummon())
        {
            me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);

            uint8 PosId = 0;

            for (uint8 Itr = 0; Itr < 5; Itr++) // We need to verify the spawn position of the creature.
            {
                if (me->GetDistance(SpawnPos[Itr]) <= 5.f) // 5 Yards just in case.
                {
                    PosId = Itr;
                    break;
                }
            }

            me->GetMotionMaster()->MoveJump(JumpPos[PosId], 10.f, 10.f, 1004U, true);

            me->CombatStart(me->SelectNearestPlayer(60.f));

            events.ScheduleEvent(EVENT_DRIVEN_ASSAULT, 2s);
            events.ScheduleEvent(EVENT_DRIVEN_ASSAULT, 4s);
        }
    }

    enum Events
    {
        EVENT_DRIVEN_ASSAULT = 1,
        EVENT_SPLASHY_CLEAVE,
    };

    void ExecuteEvent(uint32 eventId)
    {
        switch (eventId)
        {
        case EVENT_DRIVEN_ASSAULT:
        {
            me->CastSpell(nullptr, SPELL_DRIVEN_ASSAULT);
            events.Repeat(100);

            break;
        }
        case EVENT_SPLASHY_CLEAVE:
        {
            me->CastSpell(nullptr, SPELL_SPLASHY_CLEAVE);
            events.Repeat(4s);

            break;
        }
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!me->HasAura(SPELL_DRIPPING_BLADE)) // Just in case they don't spawn with the aura.
            DoCastSelf(SPELL_DRIPPING_BLADE, true);

        events.Update(diff);

        if (uint32 eventId = events.ExecuteEvent())
        {
            if (me->HasUnitState(UNIT_STATE_CASTING))
                events.RescheduleEvent(eventId, 10ms);
            else
                ExecuteEvent(eventId);

            DoMeleeAttackIfReady();
        }
    }
};

// 116569 - Razorjaw Wavemender
struct npc_razor_jaw_wavemender : public ScriptedAI
{
    npc_razor_jaw_wavemender(Creature* creature) : ScriptedAI(creature)
    {
        if (me->IsSummon())
        {
            me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);

            uint8 PosId = 0;

            for (uint8 Itr = 0; Itr < 5; Itr++) // We need to verify the spawn position of the creature.
            {
                if (me->GetDistance(SpawnPos[Itr]) <= 5.f) // 5 Yards just in case.
                {
                    PosId = Itr;
                    break;
                }
            }

            me->GetMotionMaster()->MoveJump(JumpPos[PosId], 10.f, 10.f, 1004U, true);

            me->CombatStart(me->SelectNearestPlayer(60.f));

            events.ScheduleEvent(EVENT_WATERY_SPLASH, 4s, 5s);
            events.ScheduleEvent(EVENT_AQUEOUS_BURST, 7s);
        }
    }

    enum Events
    {
        EVENT_WATERY_SPLASH = 1,
        EVENT_AQUEOUS_BURST
    };

    void ExecuteEvent(uint32 eventId)
    {
        switch (eventId)
        {
        case EVENT_WATERY_SPLASH:
        {
            me->CastSpell(SelectTarget(SELECT_TARGET_RANDOM), SPELL_WATERY_SPLASH);
            events.Repeat(4s, 5s);

            break;
        }
        case EVENT_AQUEOUS_BURST:
        {
            me->CastSpell(nullptr, SPELL_AQUEOUS_BURST);
            events.Repeat(7s);

            break;
        }
        }
    }

    void UpdateAI(uint32 diff) override
    {
        events.Update(diff);

        if (uint32 eventId = events.ExecuteEvent())
        {
            if (me->HasUnitState(UNIT_STATE_CASTING))
                events.RescheduleEvent(eventId, 10ms);
            else
                ExecuteEvent(eventId);


            DoMeleeAttackIfReady();
        }
    }
};

// 231854 - Unchecked Rage
class spell_harjatan_unchecked_rage : public SpellScript
{
    PrepareSpellScript(spell_harjatan_unchecked_rage);

    void HandleTargets(std::list<WorldObject*>& targets)
    {
        if (targets.size() < 5)
            if (Unit* caster = GetCaster())
                caster->CastSpell(caster, SPELL_ENRAGED, true);
    }

    void HandleAfterCast()
    {
        if (Unit* caster = GetCaster())
            caster->CastSpell(caster->GetVictim(), SPELL_UNCHECKED_RAGE_SINGLE);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_harjatan_unchecked_rage::HandleTargets, EFFECT_0, TARGET_UNIT_CONE_ENEMY_104);
        AfterCast += SpellCastFn(spell_harjatan_unchecked_rage::HandleAfterCast);
    }
};

// 232192 - Commanding Roar
class spell_harjatan_commanding_roar : public SpellScript
{
    PrepareSpellScript(spell_harjatan_commanding_roar);

    void HandleAfterCast()
    {
        if (Unit* caster = GetCaster())
        { 
            uint8 FormationId = urand(0, 1); 

            caster->SummonCreature(NPC_RAZORJAW_WAVEMENDER, SpawnPos[_Formations[FormationId][0]], TEMPSUMMON_CORPSE_TIMED_DESPAWN, 5000);
            caster->SummonCreature(NPC_RAZORJAW_WAVEMENDER, SpawnPos[_Formations[FormationId][1]], TEMPSUMMON_CORPSE_TIMED_DESPAWN, 5000);
            caster->SummonCreature(NPC_RAZORJAW_GLADIATOR , SpawnPos[_Formations[FormationId][2]], TEMPSUMMON_CORPSE_TIMED_DESPAWN, 5000);
            caster->SummonCreature(NPC_RAZORJAW_GLADIATOR , SpawnPos[_Formations[FormationId][3]], TEMPSUMMON_CORPSE_TIMED_DESPAWN, 5000);
        }
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_harjatan_commanding_roar::HandleAfterCast);
    }
private:
    uint8 const _Formations[2][4] = // First 2 are Razorjaw Wavemenders / Each number is an index of SpawnPos.
    { 
        {0, 1, 1, 1 }, 
        {0, 2, 3, 4 }
    };
};

// 232061 - Draw In
class aura_harjatan_draw_in : public AuraScript
{
    PrepareAuraScript(aura_harjatan_draw_in);

    bool Load() override
    {
        _MovedAtList.clear();
        return true;
    }

    void OnPeriodicProc(AuraEffect const* /*AurEff*/)
    {
        if (Unit* caster = GetCaster())
        {
            std::list<AreaTrigger*> AtList;
            std::list<AreaTrigger*> TempAtList;

            caster->GetAreaTriggerListWithSpellIDInRange(AtList, SPELL_AQUEOUS_BURST_AT, 200.f);
            caster->GetAreaTriggerListWithSpellIDInRange(TempAtList, SPELL_DRENCHING_SLOUGH_AT, 200.f);

            if(!TempAtList.empty()) // I don't want to make it extra long by using 2 lists, so merge them.
                for (AreaTrigger* at : TempAtList)
                {
                    AtList.emplace_front(at);
                }

            if (_MovedAtList.empty())
            {
                for (AreaTrigger* at : AtList)
                {
                    _MovedAtList.emplace_front(at);
                    at->SetDestination(caster->GetPosition(), 3000);
                }
            }
            else
            {
                for (AreaTrigger* at : AtList)
                {
                    bool MatchedAt = false;
                    for(AreaTrigger* movedAt : _MovedAtList)
                        if (at->GetEntry() == movedAt->GetEntry())
                        {
                            MatchedAt = true;
                            break;
                        }

                    if (MatchedAt == false)
                    {
                        _MovedAtList.emplace_front(at);
                        at->SetDestination(caster->GetPosition(), 3000);
                    }
                }
            }
            if (!AtList.empty())
            {
                for (AreaTrigger* at : AtList)
                {
                }
            }
        }
    }

    void OnUpdate(uint32 diff)
    {
        if (Unit* caster = GetCaster())
        {
            std::list<AreaTrigger*> AtList;
            std::list<AreaTrigger*> TempAtList;

            caster->GetAreaTriggerListWithSpellIDInRange(AtList, SPELL_AQUEOUS_BURST_AT, 1.f);
            caster->GetAreaTriggerListWithSpellIDInRange(TempAtList, SPELL_DRENCHING_SLOUGH_AT, 1.f);

            if (!TempAtList.empty())
                for (AreaTrigger* at : TempAtList)
                {
                    AtList.emplace_front(at);
                }

            if (!AtList.empty())
            {
                for (AreaTrigger* at : AtList)
                {
                    at->Remove();
                    caster->CastSpell(caster, SPELL_FRIGID_BLOWS_AURA, true);
                }
            }
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(aura_harjatan_draw_in::OnPeriodicProc, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
        OnAuraUpdate += AuraUpdateFn(aura_harjatan_draw_in::OnUpdate);
    }

private:
    std::list<AreaTrigger*> _MovedAtList;
};

// 232061 - Draw in
class spell_harjatan_draw_in : public SpellScript
{
    PrepareSpellScript(spell_harjatan_draw_in);

    void HandleAfterCast()
    {
        if (Unit* caster = GetCaster())
            if (!caster->HasAura(SPELL_FRIGID_BLOWS_AURA))
                caster->CastSpell(nullptr, SPELL_FROSTY_DISCHARGE); // We want to make sure that the boss casts Frosty Discharge when he doesn't have any stacks.
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_harjatan_draw_in::HandleAfterCast);
    }
};

// 233429 - Frigid Blows (Aura) 43
class aura_harjatan_frigid_blows : public AuraScript
{
    PrepareAuraScript(aura_harjatan_frigid_blows);

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Unit* caster = GetCaster())
        {
            caster->CastSpell(nullptr, SPELL_FROSTY_DISCHARGE);

            /*if(caster->GetEntry() == NPC_HARJATAN)
            */
        }
    }

    void HandleOnProc(ProcEventInfo& /*eventInfo*/)
    {
        if (Unit* caster = GetCaster())
            if (Aura* aura = GetAura())
                if (aura->GetStackAmount() >= 2)
                    aura->SetStackAmount(aura->GetStackAmount() - 1);
                else
                    caster->RemoveAurasDueToSpell(SPELL_FRIGID_BLOWS_AURA);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(aura_harjatan_frigid_blows::HandleRemove, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
        OnProc += AuraProcFn(aura_harjatan_frigid_blows::HandleOnProc);
    }
};

// 233548 - Frigid Blows (Targeting Spell)
class spell_harjatan_frigid_blows : public SpellScript
{
    PrepareSpellScript(spell_harjatan_frigid_blows);

    void HandleTargets(std::list<WorldObject*>& target)
    {
        if (target.empty())
            return;

        target.remove_if([](WorldObject* target) -> bool
        {
            return !target->IsPlayer();
        });

        if (target.empty())
            return;

        if (Unit* caster = GetCaster())
            caster->CastSpell(target.front()->ToPlayer(), SPELL_FRIGID_BLOWS_TRIGGER);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_harjatan_frigid_blows::HandleTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

// 233553 - Frigid Blows (Triggers Drenching Slough)
class spell_harjatan_frigid_blows_trigger : public SpellScript
{
    PrepareSpellScript(spell_harjatan_frigid_blows_trigger);

    void HandleAfterCast()
    {
        if (Unit* caster = GetCaster())
            if (Unit* target = GetExplTargetUnit())
                caster->CastSpell(target, SPELL_DRENCHING_SLOUGH);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_harjatan_frigid_blows_trigger::HandleAfterCast);
    }
};

// 232174 - Frosty Discharge
class spell_harjatan_frosty_discharge : public SpellScript
{
    PrepareSpellScript(spell_harjatan_frosty_discharge);

    void HandleAfterCast()
    {
        if (Unit* caster = GetCaster())
        {
            caster->CastSpell(nullptr, SPELL_CANCEL_DRENCHED_AURA);
            caster->SetPower(POWER_ENERGY, 0);
        }
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_harjatan_frosty_discharge::HandleAfterCast);
    }
};

// 239926 - Cancel Drenched Aura
class spell_harjatan_cancel_drenched_aura : public SpellScript
{
    PrepareSpellScript(spell_harjatan_cancel_drenched_aura);
    void HandleTargets(std::list<WorldObject*>& targets)
    {
        if (targets.empty())
            return;

        for (WorldObject* target : targets)
        {
            if (target->ToUnit()->HasAura(SPELL_DRENCHED))
                target->ToUnit()->RemoveAurasDueToSpell(SPELL_DRENCHED);
        }
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_harjatan_cancel_drenched_aura::HandleTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

// 240725 - Aqueous Burst (Targeting spell)
class spell_harjatan_aqueous_burst : public SpellScript
{
    PrepareSpellScript(spell_harjatan_aqueous_burst);

    void HandleTargets(std::list<WorldObject*>& target)
    {
        if (target.empty())
            return;

        Unit* Caster = GetCaster();
        if (!Caster)
            return;

        target.remove_if([](WorldObject* target) -> bool
        {
            return !target->IsPlayer() || target->ToUnit()->HasAura(SPELL_AQUEOUS_BURST_IMMUNITY);
        });

        std::list<Player*> PlayerList;
        Caster->GetPlayerListInGrid(PlayerList, 70.f);

        if(PlayerList.size() > 2)
        target.remove_if([Caster](WorldObject* target) -> bool
        {
            return Caster->GetVictim()->GetEntry() == target->GetEntry(); // We want to target tanks if there are only 2 or less players close to the boss.
        });

        if (target.empty())
            return;

        Caster->CastSpell(target.front()->ToPlayer(), SPELL_AQUEOUS_BURST_AURA, true);
        if (PlayerList.size() > 2) // We don't want to apply the immunity aura if there are 2 or less players close to the boss.
            target.front()->ToPlayer()->CastSpell(target.front()->ToPlayer(), SPELL_AQUEOUS_BURST_IMMUNITY, true); 
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_harjatan_aqueous_burst::HandleTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

// 231729 - Aqueous Burst (Aura)
class aura_harjatan_aqueous_burst : public AuraScript
{
    PrepareAuraScript(aura_harjatan_aqueous_burst);

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() == AURA_REMOVE_BY_EXPIRE)
            if (Unit* target = GetTarget())
                if (Unit* caster = GetCaster())
                    caster->CastSpell(target, SPELL_AQUEOUS_BURST_AT);
    }

    void Register() override
    {
        OnEffectRemove += AuraEffectRemoveFn(aura_harjatan_aqueous_burst::HandleRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 240789 - Driven Assault (Targeting Spell)
class spell_harjatan_driven_assault : public SpellScript
{
    PrepareSpellScript(spell_harjatan_driven_assault);

    void HandleTargets(std::list<WorldObject*>& target)
    {
        if (target.empty())
            return;

        Unit* Caster = GetCaster();
        if (!Caster)
            return;

        target.remove_if([](WorldObject* target) -> bool
        {
            return !target->IsPlayer() || target->ToUnit()->HasAura(SPELL_DRIVEN_ASSAULT_IMMUNITY);
        });

        std::list<Player*> PlayerList;
        Caster->GetPlayerListInGrid(PlayerList, 70.f);

        if (PlayerList.size() > 2)
            target.remove_if([Caster](WorldObject* target) -> bool
            {
                return Caster->GetVictim()->GetEntry() == target->GetEntry(); // We want to target tanks if there are only 2 or less players close to the boss.
            });

        if (target.empty())
            return;

        if (Player* player = target.front()->ToPlayer())
        {
            Caster->DeleteThreatList();
            Caster->CastSpell(player, SPELL_DRIVEN_ASSAULT_FIXATE, true);
            Caster->CastSpell(Caster, SPELL_DRIVEN_ASSAULT_BUFF, true);
            Caster->AddThreat(player, 1000000.f);
            Caster->SendMeleeAttackStart(player);

            if (PlayerList.size() > 2) // We don't want to apply the immunity aura if there are 2 or less players close to the boss.
                target.front()->ToPlayer()->CastSpell(target.front()->ToPlayer(), SPELL_AQUEOUS_BURST_IMMUNITY, true);
        }
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_harjatan_driven_assault::HandleTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

// 231754 - Aqueous Burst (AT)
struct at_harjatan_aqueous_burst : public AreaTriggerAI
{
    at_harjatan_aqueous_burst(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger) { }

    void OnInitialize() override
    {
        at->SetPeriodicProcTimer(5);
    }

    void OnPeriodicProc() override
    {
        if (Unit* caster = at->GetCaster())
        {
            GuidUnorderedSet const& InsideUnits = at->GetInsideUnits();
            for (ObjectGuid guid : InsideUnits)
            {
                if (Player* player = ObjectAccessor::GetPlayer(*caster, guid))
                {
                    if (at->GetDistance(player) <= 6.f)
                        player->CastSpell(player, SPELL_DRENCHING_WATERS, true);
                    if (at->GetDistance(player) >= 6.f)
                        player->RemoveAurasDueToSpell(SPELL_DRENCHING_WATERS);
                }
            }
        }
    }

    /*void OnUnitEnter(Unit* unit) override
    {
    if (unit)
    unit->CastSpell(unit, SPELL_DRENCHING_WATERS, true);
    }*/

    void OnUnitExit(Unit* unit) override
    {
        if (unit->HasAura(SPELL_DRENCHING_WATERS))
            unit->RemoveAurasDueToSpell(SPELL_DRENCHING_WATERS);
    }
};

void AddSC_boss_harjatan()
{
    RegisterCreatureAI(boss_harjatan);
    RegisterCreatureAI(npc_razor_jaw_gladiator);
    RegisterCreatureAI(npc_razor_jaw_wavemender);

    RegisterSpellScript(spell_harjatan_unchecked_rage);
    RegisterSpellScript(spell_harjatan_commanding_roar);
    RegisterSpellScript(spell_harjatan_draw_in);
    RegisterSpellScript(spell_harjatan_frigid_blows);
    RegisterSpellScript(spell_harjatan_frigid_blows_trigger);
    RegisterSpellScript(spell_harjatan_frosty_discharge);
    RegisterSpellScript(spell_harjatan_cancel_drenched_aura);
    RegisterSpellScript(spell_harjatan_aqueous_burst);
    RegisterSpellScript(spell_harjatan_driven_assault);

    RegisterAuraScript(aura_harjatan_draw_in);
    RegisterAuraScript(aura_harjatan_frigid_blows);
    RegisterAuraScript(aura_harjatan_aqueous_burst);

    RegisterAreaTriggerAI(at_harjatan_aqueous_burst);
}
