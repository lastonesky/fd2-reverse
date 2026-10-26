// Export a function inventory from the current program as TSV, optionally with
// each function's decompiled C appended.
//
// Versioned for Ghidra 12.x (GhidraScript). Run headless or via the MCP bridge:
//   run_ghidra_script({"script_name":"ExportInventory.java",
//                      "args":"/tmp/fd2-inv.tsv --decompile --min 0x10000 --max 0x50000",
//                      "timeout_seconds":600})
//
// Usage: ExportInventory.java <outFile> [--decompile] [--min 0xADDR] [--max 0xADDR]
//
// The inventory columns are:
//   address  name  size  body_bytes  xrefs  calling_convention  is_thunk  signature
// Addresses match IDA's FD2.EXE.i64 one-to-one (image base 0), so the output can be
// diffed against analysis/funcmap.csv without rebasing.

import java.io.PrintWriter;

import ghidra.app.decompiler.DecompileResults;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

public class ExportInventory extends GhidraScript {

	@Override
	public void run() throws Exception {
		String[] args = getScriptArgs();
		if (args.length < 1) {
			println("usage: ExportInventory.java <outFile> [--decompile] [--min 0xADDR] [--max 0xADDR]");
			return;
		}

		String outFile = args[0];
		boolean withC = false;
		Address min = null;
		Address max = null;

		for (int i = 1; i < args.length; i++) {
			String a = args[i];
			if (a.equals("--decompile")) {
				withC = true;
			}
			else if (a.equals("--min") && i + 1 < args.length) {
				min = toAddr(args[++i]);
			}
			else if (a.equals("--max") && i + 1 < args.length) {
				max = toAddr(args[++i]);
			}
			else {
				println("ignoring unknown argument: " + a);
			}
		}

		DecompInterface ifc = null;
		if (withC) {
			ifc = new DecompInterface();
			ifc.toggleCCode(true);
			ifc.toggleSyntaxTree(true);
			if (!ifc.openProgram(currentProgram)) {
				println("WARNING: decompiler failed to open the program; writing inventory only");
				withC = false;
			}
		}

		int n = 0;
		int failed = 0;
		try (PrintWriter out = new PrintWriter(outFile, "UTF-8")) {
			out.println("address\tname\tsize\tbody_bytes\txrefs\tcalling_convention\tis_thunk\tsignature");

			FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
			while (it.hasNext() && !monitor.isCancelled()) {
				Function f = it.next();
				Address entry = f.getEntryPoint();
				if (min != null && entry.compareTo(min) < 0) {
					continue;
				}
				if (max != null && entry.compareTo(max) >= 0) {
					continue;
				}

				int xrefs = currentProgram.getReferenceManager().getReferenceCountTo(entry);
				out.printf("%s\t%s\t%d\t%d\t%d\t%s\t%s\t%s%n",
					entry, f.getName(), f.getBody().getNumAddresses(), f.getBody().getNumAddresses(),
					xrefs, f.getCallingConventionName(), f.isThunk(),
					f.getPrototypeString(false, false));
				n++;

				if (withC) {
					out.println();
					out.println("// ------------------------------------------------------------------");
					out.println("// " + entry + "  " + f.getName() + "  " + f.getPrototypeString(false, false));
					out.println("// ------------------------------------------------------------------");
					DecompileResults res = ifc.decompileFunction(f, 60, monitor);
					if (res != null && res.decompileCompleted() && res.getDecompiledFunction() != null) {
						out.println(res.getDecompiledFunction().getC());
					}
					else {
						failed++;
						out.println("// DECOMPILE FAILED: " + (res == null ? "null result" : res.getErrorMessage()));
					}
					out.println();
				}
			}
		}
		finally {
			if (ifc != null) {
				ifc.dispose();
			}
		}

		println("ExportInventory: wrote " + n + " functions to " + outFile
			+ (withC ? " (with decompiled C, " + failed + " failures)" : ""));
	}
}
