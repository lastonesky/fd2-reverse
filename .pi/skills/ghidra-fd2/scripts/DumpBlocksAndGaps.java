// Dump the memory map (blocks + address spaces) and the executable code gaps of
// the current program.
//
// Why: FD2.EXE is a Watcom DOS/4GW LE image whose chkstk prologues and jump tables
// make Ghidra's recursive descent stop early. A plain reanalyze yields ~706
// functions where IDA's FD2.EXE.i64 has ~1369, and whole ranges (e.g. 0x2b843
// ..0x2cf30, which contains the known function 0x2C67D) stay undefined. This
// script reports exactly which ranges are still not code, so the gaps can be
// filled with create_function(addr) and cross-checked against IDA.
//
// Usage: DumpBlocksAndGaps.java <outFile> [--min-gap 16] [--follow-flow false]
//
// Addresses match IDA one-to-one (image base 0). Nothing is modified.

import java.io.PrintWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressRange;
import ghidra.program.model.address.AddressRangeIterator;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.address.AddressSpace;
import ghidra.program.model.mem.MemoryBlock;

public class DumpBlocksAndGaps extends GhidraScript {

	@Override
	public void run() throws Exception {
		String[] args = getScriptArgs();
		if (args.length < 1) {
			println("usage: DumpBlocksAndGaps.java <outFile> [--min-gap 16] [--follow-flow true|false]");
			return;
		}

		String outFile = args[0];
		long minGap = 16;
		boolean followFlow = true;

		for (int i = 1; i < args.length; i++) {
			String a = args[i];
			if (a.equals("--min-gap") && i + 1 < args.length) {
				minGap = Long.parseLong(args[++i]);
			}
			else if (a.equals("--follow-flow") && i + 1 < args.length) {
				followFlow = Boolean.parseBoolean(args[++i]);
			}
			else {
				println("ignoring unknown argument: " + a);
			}
		}

		try (PrintWriter out = new PrintWriter(outFile, "UTF-8")) {
			out.println("# program: " + currentProgram.getName());
			out.println("# language: " + currentProgram.getLanguageID()
				+ "  image_base: " + currentProgram.getImageBase());
			out.println();

			out.println("## address spaces");
			for (AddressSpace sp : currentProgram.getAddressFactory().getAddressSpaces()) {
				out.printf("%-16s %s  unit_size=%d bits  overlay=%s%n", sp.getName(),
					sp.getSize() + " bits", sp.getAddressableUnitSize(), sp.isOverlaySpace());
			}
			out.println();

			out.println("## memory blocks");
			out.println("space\tstart\tend\tsize\tname\ttype\tR\tW\tX\tinitialized");
			AddressSet executable = new AddressSet();
			for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
				out.printf("%s\t%s\t%s\t%d\t%s\t%s\t%s\t%s\t%s\t%s%n",
					b.getStart().getAddressSpace().getName(),
					b.getStart(), b.getEnd(), b.getSize(), b.getName(), b.getType(),
					b.isRead(), b.isWrite(), b.isExecute(), b.isInitialized());
				if (b.isExecute() && b.isInitialized()) {
					executable.add(b.getAddressRange());
				}
			}
			out.println();

			out.println("## executable ranges: " + executable.getNumAddressRanges()
				+ " range(s), " + executable.getNumAddresses() + " bytes");

			AddressSetView undefined =
				currentProgram.getListing().getUndefinedRanges(executable, followFlow, monitor);
			if (undefined == null) {
				out.println("## gaps: <not computed>");
				println("DumpBlocksAndGaps: wrote " + outFile + " (gaps unavailable)");
				return;
			}

			int reported = 0;
			long total = 0;
			out.println();
			out.println("## code gaps (undefined, >= " + minGap + " bytes)");
			out.println("start\tend\tbytes");
			AddressRangeIterator it = undefined.getAddressRanges();
			while (it.hasNext() && !monitor.isCancelled()) {
				AddressRange r = it.next();
				long len = r.getLength();
				if (len < minGap) {
					continue;
				}
				reported++;
				total += len;
				out.printf("%s\t%s\t%d%n", r.getMinAddress(), r.getMaxAddress(), len);
			}
			out.println();
			out.println("# gaps reported: " + reported + "  bytes undefined: " + total
				+ "  functions known: " + currentProgram.getFunctionManager().getFunctionCount());

			println("DumpBlocksAndGaps: " + reported + " gap(s), " + total + " bytes, "
				+ currentProgram.getFunctionManager().getFunctionCount() + " functions -> " + outFile);
		}
	}
}
