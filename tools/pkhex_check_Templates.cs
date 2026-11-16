// Templates.cs - for every course Pokemon of a PocketPKMN route file and every game version, let PKHeX find a legal
// way it could have been obtained in that game (hidden ability first, then wild, then egg, then the rest), build it at
// the course level with the course moves, and keep it only if PKHeX's own legality check passes.
//   pkhex-check templates <routes.txt> <out.bin> <report.md> <versions...>   (versions: GD SI C | X Y | SN MN US UM)
// out.bin: "PPCT" u16 version-count, then per entry: u8 version index, u8 course, u8 poke (0-5), u8 gen, u8 flags
// (1 = hidden ability, 2 = egg, 4 = traded history), u8 fixed IV count, u16 data length, data (PK6 / PK7: 232-byte
// decrypted box data; PK2: 32-byte box data). A course Pokemon without a legal origin has no entry (and is listed in
// the report). Gen 7 route lines (tools/routes_sm.py) also carry form=N, origin=<games> and ha=0/1: the catch comes
// from the player's game if it is in <games>, else its pair (Sun <-> Ultra Sun, Moon <-> Ultra Moon), else the first
// listed (X/Y = a Gen 6 Pokemon brought over the Bank way); ha=1 = hidden ability, ha=0 = never.
using System.Text;
using PKHeX.Core;

public static class Templates
{
    record Poke(int Course, int Index, string CourseName, ushort Species, byte Level, ushort[] Moves, string Name,
                byte Form = 0, string[]? Origins = null, int Ha = -1);

    static List<Poke> ReadRoutes(string path)
    {
        var list = new List<Poke>();
        int course = -1, idx = 0;
        string cname = "";
        foreach (var raw in File.ReadAllLines(path))
        {
            var line = raw.Split(';')[0].Trim();
            var comment = raw.Contains(';') ? raw[(raw.IndexOf(';') + 1)..].Trim() : "";
            if (line.StartsWith("course "))
            {
                var parts = line.Split('|');
                course = int.Parse(parts[0].Split(' ', StringSplitOptions.RemoveEmptyEntries)[1]);
                cname = parts[1].Trim();
                idx = 0;
            }
            else if (line.StartsWith("poke "))
            {
                var f = line.Split(' ', StringSplitOptions.RemoveEmptyEntries);
                ushort sp = ushort.Parse(f[4]);
                byte lv = byte.Parse(f[5]);
                var mv = f.Skip(7).Take(4).Select(ushort.Parse).Where(m => m != 0).ToArray();
                string? Tok(string key) => f.Skip(11).FirstOrDefault(t => t.StartsWith(key))?[key.Length..];
                list.Add(new Poke(course, idx++, cname, sp, lv, mv, comment.Split(':')[0],
                    byte.Parse(Tok("form=") ?? "0"), Tok("origin=")?.Split(','), int.Parse(Tok("ha=") ?? "-1")));
            }
        }
        return list;
    }

    static int Gen(GameVersion v) => v is GameVersion.GD or GameVersion.SI or GameVersion.C ? 2 :
        v is GameVersion.D or GameVersion.P or GameVersion.Pt ? 4 :
        v is GameVersion.SN or GameVersion.MN or GameVersion.US or GameVersion.UM ? 7 : 6;

    static GameVersion Ver(string s) => s switch
    {
        "GD" => GameVersion.GD, "SI" => GameVersion.SI, "C" => GameVersion.C, "X" => GameVersion.X, "Y" => GameVersion.Y,
        "SN" => GameVersion.SN, "MN" => GameVersion.MN, "US" => GameVersion.US, "UM" => GameVersion.UM,
        "D" => GameVersion.D, "P" => GameVersion.P, "Pt" => GameVersion.Pt,
        _ => throw new ArgumentException(s),
    };

    // the game a catch comes from for a player of game v (see the header)
    static GameVersion Origin(Poke p, GameVersion v)
    {
        if (p.Origins is null || p.Origins.Length == 0)
            return v;
        var list = p.Origins.Select(Ver).ToList();
        if (list.Contains(v))
            return v;
        GameVersion pair = v switch
        {
            GameVersion.SN => GameVersion.US, GameVersion.US => GameVersion.SN,
            GameVersion.MN => GameVersion.UM, GameVersion.UM => GameVersion.MN, _ => v,
        };
        if (list.Contains(pair))
            return pair;
        bool moonSide = v is GameVersion.MN or GameVersion.UM; // Gen 6 origins: Sun side -> X, Moon side -> Y
        if (list.Contains(GameVersion.X) && list.Contains(GameVersion.Y))
            return moonSide ? GameVersion.Y : GameVersion.X;
        return list[0];
    }

    // The trainer the template is made for; the app replaces OT name / IDs / region with the user's own.
    static SimpleTrainerInfo Trainer(GameVersion v) => new(v)
    {
        OT = Gen(v) == 2 ? "PLAYER" : "PLAYER",
        TID16 = 12345, SID16 = (ushort)(Gen(v) == 2 ? 0 : 54321), Gender = 0, Language = (int)LanguageID.English,
        Country = 49, Region = 7, ConsoleRegion = 1,
    };

    static SimpleTrainerInfo OtherTrainer(GameVersion v) => new(v)
    {
        OT = "Trader", TID16 = 22222, SID16 = 33333, Gender = 0, Language = (int)LanguageID.English,
        Country = 49, Region = 7, ConsoleRegion = 1,
    };

    // Turn an encounter into the course Pokemon; null if PKHeX does not call it legal.
    static PKM? Build(IEncounterable enc, Poke p, GameVersion v, bool wantHidden, out bool traded, out string why,
                      int target = 0)
    {
        traded = false;
        why = "";
        if (enc is not IEncounterConvertible conv)
        {
            why = "not convertible";
            return null;
        }
        var tr = Trainer(v);
        var crit = EncounterCriteria.Unrestricted with
        {
            Ability = wantHidden ? AbilityPermission.OnlyHidden : AbilityPermission.Any12,
        };
        PKM pk;
        try { pk = conv.ConvertToPKM(tr, crit); }
        catch (Exception e) { why = "convert: " + e.Message; return null; }
        if (wantHidden && pk.AbilityNumber != 4)
        {
            why = "no hidden ability from this encounter";
            return null;
        }
        if (p.Ha == 0 && pk.AbilityNumber == 4)
        {
            why = "hidden ability not wanted (ha=0)";
            return null;
        }
        if (pk.Species != p.Species) // evolve to the course species, keeping the ability slot
        {
            int slot = pk.AbilityNumber >> 1;
            pk.Species = p.Species;
            if (p.Form != 0)
                pk.Form = p.Form;
            pk.RefreshAbility(slot);
            pk.Gender = pk.GetSaneGender();
        }
        if (target == 7 && pk.Format < 7) // a Gen 6 origin brought into Gen 7 the Bank way
        {
            var moved = EntityConverter.ConvertToType(pk, typeof(PK7), out var res);
            if (moved is null)
            {
                why = "to Gen 7: " + res;
                return null;
            }
            pk = moved;
            // after the move the Gen 7 game lists its player as the handler, even the same trainer (PKHeX:
            // "Current handler cannot be the OT" otherwise); the app puts the player's name in both places
            pk.HandlingTrainerName = tr.OT;
            pk.HandlingTrainerGender = (byte)tr.Gender;
            pk.CurrentHandler = 1;
            pk.RefreshChecksum();
            if (Environment.GetEnvironmentVariable("TPLDEBUG") == "1")
                Console.WriteLine($"dbg moved: OT {pk.OriginalTrainerName} HT '{pk.HandlingTrainerName}' cur {pk.CurrentHandler} " +
                                  $"ver {pk.Version} | " + new LegalityAnalysis(pk).Report().Replace('\n', '/'));
        }
        if (pk.CurrentLevel < p.Level)
            pk.CurrentLevel = p.Level;
        if (pk.IsEgg)
        {
            why = "still an egg";
            return null;
        }
        Span<ushort> mv = stackalloc ushort[4];
        p.Moves.CopyTo(mv);
        pk.SetMoves(mv);
        pk.HealPP();
        pk.ClearNickname();
        var la = new LegalityAnalysis(pk);
        if (pk.Format >= 6)
        {
            Span<ushort> rel = stackalloc ushort[4];
            MoveListSuggest.GetSuggestedRelearnMovesFromEncounter(la, rel, la.EncounterOriginal);
            pk.SetRelearnMoves(rel);
            la = new LegalityAnalysis(pk);
        }
        if (!la.Valid && pk.Format >= 7) // Gen 7 without a trade: try no relearn moves
        {
            var keep = pk.RelearnMoves.ToArray();
            pk.SetRelearnMoves(new ushort[4]);
            pk.RefreshChecksum();
            var la0 = new LegalityAnalysis(pk);
            if (la0.Valid)
                return pk;
            pk.SetRelearnMoves(keep);
        }
        if (!la.Valid && pk is IHandlerUpdate h) // a trade evolution on the way: a traded-once history
        {
            var other = OtherTrainer(v);
            h.UpdateHandler(other);
            h.UpdateHandler(tr);
            pk.RefreshChecksum();
            var la2 = new LegalityAnalysis(pk);
            if (la2.Valid)
            {
                traded = true;
                return pk;
            }
            if (pk.Format >= 7) // Gen 7: the relearn moves the traded Pokemon's own encounter wants, else none
            {
                var relT = new ushort[4];
                MoveListSuggest.GetSuggestedRelearnMovesFromEncounter(la2, relT, la2.EncounterOriginal);
                foreach (var r in new[] { relT, new ushort[4] })
                {
                    pk.SetRelearnMoves(r);
                    pk.RefreshChecksum();
                    if (new LegalityAnalysis(pk).Valid)
                    {
                        traded = true;
                        return pk;
                    }
                }
            }
        }
        if (!la.Valid)
        {
            why = string.Join(" | ", la.Report().Split('\n').Select(s => s.Trim()).Where(s => s.Length > 0 &&
                !s.StartsWith("Valid", StringComparison.OrdinalIgnoreCase)).Take(3));
            return null;
        }
        pk.RefreshChecksum();
        return pk;
    }

    static int Rank(IEncounterable e, byte level, int gen = 0)
    {
        // Gen 4 (v0.61): eggs first - a hatched Gen 4 Pokemon's PID and IVs are free (the app may pick gender and
        // shininess), a wild one's are tied to the game's RNG method and must stay as PKHeX made them
        if (gen == 4)
            return e.IsEgg ? 0 : e.GetType().Name.StartsWith("EncounterSlot") ? 1 : 3;
        // hidden-ability-capable first, then wild slots, then eggs, then statics/trades/gifts
        int r = e is EncounterSlot6XY s && s.IsHorde ? 0 : 0;
        if (e.IsEgg) return 2;
        if (e.GetType().Name.StartsWith("EncounterSlot")) return 1;
        return 3;
    }

    public static int Run(string routes, string outBin, string report, string[] versions)
    {
        var pokes = ReadRoutes(routes);
        var md = new StringBuilder();
        md.AppendLine($"# Legal catch templates for {Path.GetFileName(routes)}");
        md.AppendLine();
        md.AppendLine("Generated by tools/pkhex_check (PKHeX.Core " +
                      typeof(PKM).Assembly.GetName().Version + "): every row passed PKHeX's legality check.");
        md.AppendLine();
        md.AppendLine("| Course | Pokemon | Lv | " + string.Join(" | ", versions) + " |");
        md.AppendLine("|---|---|---|" + string.Concat(versions.Select(_ => "---|")));
        using var ms = new MemoryStream();
        using var bw = new BinaryWriter(ms);
        bw.Write(Encoding.ASCII.GetBytes("PPCT"));
        bw.Write((ushort)versions.Length);
        int missing = 0, total = 0;
        foreach (var p in pokes)
        {
            var cells = new List<string>();
            for (int vi = 0; vi < versions.Length; vi++)
            {
                var player = Ver(versions[vi]);
                var v = Origin(p, player); // = player except for Gen 7 lines with origin=
                int target = Gen(player) == 7 ? 7 : 0;
                total++;
                PKM blank = Gen(player) == 2 ? new PK2() : Gen(player) == 4 ? new PK4() : Gen(player) == 7 ? new PK7() : new PK6();
                blank.Species = p.Species;
                blank.Form = p.Form;
                blank.CurrentLevel = p.Level;
                if (Gen(player) == 4)
                    blank.Gender = blank.GetSaneGender(); // female-only species (Froslass) need a female blank
                IEnumerable<IEncounterable> encs;
                try { encs = EncounterMovesetGenerator.GenerateEncounters(blank, p.Moves, v).ToList(); }
                catch (Exception e) { cells.Add("ERROR " + e.Message); missing++; continue; }
                var usable = encs.Where(e => e.IsEgg || e.LevelMin <= p.Level)
                                 .OrderBy(e => Rank(e, p.Level, Gen(player) == 4 ? 4 : 0)).ToList();
                PKM? best = null;
                IEncounterable? from = null;
                bool hidden = false, traded = false;
                string lastWhy = usable.Count == 0 ? "no encounter at or below Lv " + p.Level : "";
                if (Gen(v) >= 6 && p.Ha != 0) // hidden ability first (user: "ideally, I'd like catches to have hidden abilities")
                    foreach (var e in usable)
                    {
                        best = Build(e, p, v, true, out traded, out lastWhy, target);
                        if (best != null) { from = e; hidden = true; break; }
                    }
                if (best == null && p.Ha == 1)
                    Console.WriteLine($"note: {p.CourseName} {p.Name} ({player}): no legal hidden ability ({lastWhy})");
                // v0.66: a Gen 4 egg is tried several times - a one-gender evolution (Froslass) needs the egg to have
                // hatched as that gender, and one unlucky try sent D / P Froslass to a wild origin (which the app
                // cannot make shiny). As done to the existing file by "pkhex-check egg4".
                int attempts = Gen(player) == 4 ? 40 : 1;
                if (best == null)
                    foreach (var e in usable)
                    {
                        for (int a = 0; a < (e.IsEgg ? attempts : 1) && best == null; a++)
                            best = Build(e, p, v, false, out traded, out lastWhy, target);
                        if (best != null) { from = e; break; }
                    }
                if (best == null || from == null)
                {
                    missing++;
                    cells.Add("**NO LEGAL ORIGIN** (" + lastWhy.Replace("|", "/") + ")");
                    continue;
                }
                // v0.64 (review 2026-11, finding 2): a Gen 7 catch whose origin is ANOTHER Gen 7 game (Mantyke in Moon
                // comes from Ultra Moon ...) can only be in the player's game after a trade, and then the player is its
                // current handler. Without this PKHeX, with the save open, says "Invalid Current handler value" (152 of
                // 432 Moon catches). Same marking as the X / Y Ditto above; the app puts the player's name and gender
                // in place of "PLAYER" (source/walk_catch.cpp).
                if (target == 7 && v != player && best.Format == 7 && Gen(v) == 7)
                {
                    var me = Trainer(player);
                    best.HandlingTrainerName = me.OT;
                    best.HandlingTrainerGender = (byte)me.Gender;
                    best.CurrentHandler = 1;
                    best.RefreshChecksum();
                    var laH = new LegalityAnalysis(best);
                    if (!laH.Valid)
                    {
                        missing++;
                        cells.Add("**NOT LEGAL AS RECEIVED IN A TRADE** (" + string.Join(" / ", laH.Report().Split('\n')
                            .Select(s => s.Trim()).Where(s => s.StartsWith("Invalid")).Take(3)) + ")");
                        continue;
                    }
                }
                int fixedIVs = from is IFixedIVSet { IVs.IsSpecified: true } ? 6 :
                               from is IFlawlessIVCount f ? f.FlawlessIVCount : 0;
                // Gen 7: a wild Pokemon with its hidden ability comes from an SOS chain, which PKHeX requires to have
                // at least 2 perfect IVs ("Should have at least 2 IVs = 31")
                if (target == 7 && hidden && !from.IsEgg && from is EncounterSlot7)
                    fixedIVs = Math.Max(fixedIVs, 2);
                byte flags = (byte)((hidden ? 1 : 0) | (from.IsEgg ? 2 : 0) | (traded ? 4 : 0));
                byte[] data = best is PK2 pk2 ? pk2.Data.ToArray()[..32]
                            : best is PK4 ? best.Data.ToArray()[..136] : best.Data.ToArray()[..232]; // PK4: 136-byte box data
                bw.Write((byte)vi); bw.Write((byte)p.Course); bw.Write((byte)p.Index); bw.Write((byte)best.Format);
                bw.Write(flags); bw.Write((byte)fixedIVs); bw.Write((ushort)data.Length); bw.Write(data);
                string loc = from.IsEgg ? "egg" : GameInfo.GetLocationName(false, best.MetLocation, best.Format,
                    target == 7 ? best.Generation : best.Format, v);
                if (from.IsEgg && target == 7)
                    loc = "egg hatched at " + GameInfo.GetLocationName(false, best.MetLocation, 7, 7, v);
                cells.Add((v != player ? $"({v}) " : "") + $"{from.LongName.Replace("|", "/")} - met Lv {best.MetLevel} at {loc}" +
                          (hidden ? " - **hidden ability**" : "") + (traded ? " - traded once" : ""));
            }
            md.AppendLine($"| {p.Course} {p.CourseName} | {p.Name} | {p.Level} | " + string.Join(" | ", cells) + " |");
        }
        File.WriteAllBytes(outBin, ms.ToArray());
        md.Insert(0, "");
        md.AppendLine();
        md.AppendLine($"{total - missing} of {total} have a legal origin; {missing} do not.");
        File.WriteAllText(report, md.ToString());
        Console.WriteLine($"{total - missing}/{total} legal, {missing} without a legal origin -> {report}");
        return missing > 0 ? 1 : 0;
    }
}
