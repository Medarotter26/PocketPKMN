// Gen7Check.cs - check the Gen 7 course sheet against PKHeX: for each row and each of SN MN US UM, can PKHeX build a
// legal Pokemon of that species/form at that level, with the hidden ability and without? Where from (met place/level)?
//   pkhex-check gen7 <rows.tsv> <items.txt>
using PKHeX.Core;

public static class Gen7Check
{
    static readonly GameVersion[] Vers = { GameVersion.SN, GameVersion.MN, GameVersion.US, GameVersion.UM };

    static SimpleTrainerInfo Trainer(GameVersion v) => new(v)
    {
        OT = "PLAYER", TID16 = 12345, SID16 = 54321, Gender = 0, Language = (int)LanguageID.English,
        Country = 49, Region = 7, ConsoleRegion = 1,
    };

    static (PKM? pk, string info) Try(ushort sp, byte form, byte level, GameVersion v, bool hidden)
    {
        PKM blank = new PK7 { Species = sp, Form = form, CurrentLevel = level };
        List<IEncounterable> encs;
        try { encs = EncounterMovesetGenerator.GenerateEncounters(blank, ReadOnlyMemory<ushort>.Empty, v).ToList(); }
        catch (Exception e) { return (null, "error " + e.Message); }
        encs = encs.Where(e => e.IsEgg || e.LevelMin <= level).ToList();
        if (encs.Count == 0) return (null, "no encounter at or below Lv " + level);
        string last = "";
        // wild first, then egg, then gifts/statics
        foreach (var enc in encs.OrderBy(e => e.IsEgg ? 1 : e.GetType().Name.StartsWith("EncounterSlot") ? 0 : 2))
        {
            if (enc is not IEncounterConvertible conv) continue;
            var crit = EncounterCriteria.Unrestricted with
            { Ability = hidden ? AbilityPermission.OnlyHidden : AbilityPermission.Any12 };
            PKM pk;
            try { pk = conv.ConvertToPKM(Trainer(v), crit); } catch (Exception e) { last = e.Message; continue; }
            if (hidden && pk.AbilityNumber != 4) { last = "no HA from " + enc.LongName; continue; }
            if (!hidden && pk.AbilityNumber == 4) { last = "only HA from " + enc.LongName; continue; }
            if (pk.Species != sp)
            {
                int slot = pk.AbilityNumber >> 1;
                pk.Species = sp;
                pk.Form = form;
                pk.RefreshAbility(slot);
                pk.Gender = pk.GetSaneGender();
            }
            if (pk.IsEgg) { last = "egg"; continue; }
            if (pk.CurrentLevel < level) pk.CurrentLevel = level;
            pk.ClearNickname();
            var la = new LegalityAnalysis(pk);
            var mv = new ushort[4];
            la.GetSuggestedCurrentMoves(mv, MoveSourceType.All);
            pk.SetMoves(mv);
            pk.HealPP();
            la = new LegalityAnalysis(pk);
            var rel = new ushort[4];
            MoveListSuggest.GetSuggestedRelearnMovesFromEncounter(la, rel, la.EncounterOriginal);
            pk.SetRelearnMoves(rel);
            pk.RefreshChecksum();
            la = new LegalityAnalysis(pk);
            if (!la.Valid) // relearn moves the encounter does not have: try none
            {
                pk.SetRelearnMoves(new ushort[4]);
                pk.RefreshChecksum();
                var la0 = new LegalityAnalysis(pk);
                if (la0.Valid) la = la0;
                else pk.SetRelearnMoves(rel);
            }
            if (Environment.GetEnvironmentVariable("G7DEBUG") == "1" && !la.Valid)
                Console.WriteLine($"     dbg {v} {(hidden ? "HA" : "reg")} {enc.LongName}: " + la.Report().Replace('\n', '/'));
            if (!la.Valid && pk is IHandlerUpdate h)
            {
                h.UpdateHandler(new SimpleTrainerInfo(v) { OT = "Trader", TID16 = 2, SID16 = 3, Language = 2 });
                h.UpdateHandler(Trainer(v));
                pk.RefreshChecksum();
                var laT = new LegalityAnalysis(pk);
                var relT = new ushort[4];
                MoveListSuggest.GetSuggestedRelearnMovesFromEncounter(laT, relT, laT.EncounterOriginal);
                foreach (var r in new[] { relT, rel, new ushort[4] })
                {
                    pk.SetRelearnMoves(r);
                    pk.RefreshChecksum();
                    var la2 = new LegalityAnalysis(pk);
                    if (la2.Valid)
                        return (pk, Describe(enc, pk, v) + " +traded once");
                    la = la2;
                }
            }
            if (!la.Valid)
            {
                last = string.Join(" / ", la.Report().Split('\n').Select(s => s.Trim())
                    .Where(s => s.Length > 0 && !s.StartsWith("Valid")).Take(2));
                continue;
            }
            return (pk, Describe(enc, pk, v));
        }
        return (null, last);
    }

    static string Describe(IEncounterable enc, PKM pk, GameVersion v)
    {
        string loc = enc.IsEgg ? "egg, hatched at " + GameInfo.GetLocationName(false, pk.MetLocation, 7, 7, v)
                               : GameInfo.GetLocationName(false, pk.MetLocation, 7, pk.Generation, v);
        string ab = GameInfo.Strings.Ability[pk.Ability];
        return $"{enc.GetType().Name.Replace("Encounter", "")} met Lv{pk.MetLevel} at {loc} [{ab}]";
    }

    // where PKHeX keeps the Hall of Fame (first entry) and the Z-Crystal pouch in a Sun/Moon and an Ultra save
    public static int Probe()
    {
        foreach (SAV7 sav in new SAV7[] { new SAV7SM(), new SAV7USUM() })
        {
            var a = sav.Data.ToArray();
            sav.EventWork.Fame.First1 = 0x1234;
            var b = sav.Data.ToArray();
            for (int i = 0; i < a.Length; i++)
                if (a[i] != b[i]) Console.WriteLine($"{sav.GetType().Name}: Fame.First1 byte at 0x{i:X}");
        }
        foreach (var n in new[] { "Normalium Z", "Fightinium Z", "Waterium Z", "Firium Z", "Grassium Z", "Rockium Z",
                                  "Electrium Z", "Ghostium Z", "Darkinium Z", "Dragonium Z", "Groundium Z" })
            Console.WriteLine($"{n}: " + string.Join(",", Enumerable.Range(0, GameInfo.Strings.Item.Count)
                .Where(i => GameInfo.Strings.Item[i] == n)));
        // the item numbers the Z-Crystal pouch may hold (bag form), with their names
        foreach (SAV7 sav in new SAV7[] { new SAV7SM(), new SAV7USUM() })
            foreach (var p in sav.Inventory.Pouches.Where(p => p.Type == InventoryType.ZCrystals))
            {
                var legal = p.GetAllItems();
                Console.WriteLine($"{sav.GetType().Name} Z pouch: " + string.Join(", ", legal.ToArray().Select(i =>
                    $"{i}={GameInfo.Strings.Item[i]}")));
            }
        return 0;
    }

    // which bytes differ between two Gen 7 saves, by PKHeX's save blocks (and the block table / signature after them)
    public static int Diff(string a, string b)
    {
        var sa = SaveUtil.GetSaveFile(a) as SAV7;
        var sb = SaveUtil.GetSaveFile(b) as SAV7;
        if (sa is null || sb is null) { Console.WriteLine("not Gen 7 saves"); return 2; }
        var da = File.ReadAllBytes(a);
        var db = File.ReadAllBytes(b);
        Console.WriteLine($"{sa.Version} checksums valid: before {sa.ChecksumsValid}, after {sb.ChecksumsValid}");
        var blocks = sa.AllBlocks;
        int end = 0;
        for (int i = 0; i < blocks.Count; i++)
        {
            var bl = blocks[i];
            int n = 0, first = -1, last = -1;
            for (int k = bl.Offset; k < bl.Offset + bl.Length && k < da.Length; k++)
                if (da[k] != db[k]) { n++; if (first < 0) first = k; last = k; }
            end = Math.Max(end, bl.Offset + bl.Length);
            if (n > 0)
                Console.WriteLine($"block {i,2} at 0x{bl.Offset:X5} len 0x{bl.Length:X5}: {n} byte(s) differ (0x{first:X5}-0x{last:X5})");
        }
        int tail = 0, tf = -1, tl = -1;
        for (int k = end; k < da.Length; k++)
            if (da[k] != db[k]) { tail++; if (tf < 0) tf = k; tl = k; }
        Console.WriteLine($"after the last block (0x{end:X5}..): {tail} byte(s) differ" + (tail > 0 ? $" (0x{tf:X5}-0x{tl:X5})" : ""));
        // the boxes: which slots changed
        for (int i = 0; i < sa.SlotCount; i++)
        {
            var pa = sa.GetBoxSlotAtIndex(i);
            var pb = sb.GetBoxSlotAtIndex(i);
            if (!pa.Data.ToArray().AsSpan(0, 232).SequenceEqual(pb.Data.ToArray().AsSpan(0, 232)))
                Console.WriteLine($"box {i / 30 + 1} slot {i % 30 + 1}: {GameInfo.Strings.Species[pa.Species]} (EC {pa.EncryptionConstant:X8}) -> " +
                                  $"{GameInfo.Strings.Species[pb.Species]} (EC {pb.EncryptionConstant:X8})");
        }
        return 0;
    }

    public static int Run(string tsv, string itemsFile)
    {
        var s = GameInfo.Strings;
        foreach (var raw in File.ReadAllLines(tsv))
        {
            var f = raw.Split('\t');
            string course = f[2], name = f[5], ha = f[8], vers = f[10];
            byte form = byte.Parse(f[6]), level = byte.Parse(f[7]);
            bool give = f[9] == "1";
            int sp = Array.FindIndex(s.Species.ToArray(), x => x == name);
            if (sp <= 0) { Console.WriteLine($"ROW {course} | {name}: SPECIES NOT FOUND"); continue; }
            var pi = PersonalTable.USUM.GetFormEntry((ushort)sp, form);
            string realHA = s.Ability[pi.AbilityH];
            string abil = $"{s.Ability[pi.Ability1]}/{s.Ability[pi.Ability2]}/HA {realHA}";
            bool formOk = form == 0 || pi.FormCount > form;
            Console.WriteLine($"ROW {course} | {name} f{form} Lv{level} | sheet HA '{ha}' give={give} origin={vers} | game: {abil}" +
                              (formOk ? "" : " | FORM NOT IN GAME") +
                              (ha != "None" && ha != realHA ? " | HA NAME DIFFERS" : "") +
                              (ha == "None" && pi.AbilityH != pi.Ability1 ? " | SHEET SAYS NO HA BUT IT HAS ONE" : ""));
            var vlist = Environment.GetEnvironmentVariable("G7VERS") is { Length: > 0 } ev
                ? ev.Split(',').Select(Enum.Parse<GameVersion>).ToArray() : Vers;
            foreach (var v in vlist)
            {
                var (h, hi) = Try((ushort)sp, form, level, v, true);
                var (r, ri) = Try((ushort)sp, form, level, v, false);
                Console.WriteLine($"   {v}: HA {(h != null ? "OK " + hi : "no (" + hi + ")")}");
                Console.WriteLine($"   {v}: reg {(r != null ? "OK " + ri : "no (" + ri + ")")}");
            }
        }
        // items
        var list = s.Item.ToArray();
        foreach (var raw in File.ReadAllLines(itemsFile))
        {
            var n = raw.Trim();
            if (n.Length == 0) continue;
            var ids = Enumerable.Range(0, list.Length).Where(i => string.Equals(list[i], n, StringComparison.OrdinalIgnoreCase)).ToList();
            Console.WriteLine($"ITEM {n}: " + (ids.Count == 0 ? "NOT FOUND" :
                string.Join(",", ids.Select(i => i + (i > 920 ? " (USUM only)" : "")))));
        }
        return 0;
    }
}
