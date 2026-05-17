#include "catch.hpp"

// Test that the DuckDB CLI defaults bail_on_error=true for non-interactive
// (piped) stdin.  This is a POSIX-only test: it spawns the duckdb binary with a
// redirected file as stdin and inspects stdout.
#ifndef _WIN32
#include <fcntl.h>
#include <string>
#include <unistd.h>

static std::string RunDuckDBWithInput(const std::string &sql) {
	std::string duckdb_binary = std::string(DUCKDB_BUILD_DIRECTORY) + "/duckdb";

	// Write the SQL script to a temp file so it can be used as a
	// non-interactive stdin (isatty(0) == 0).
	char tmpname[] = "/tmp/duckdb_bail_XXXXXX";
	int fd = mkstemp(tmpname);
	if (fd == -1) {
		return "";
	}
	(void)write(fd, sql.c_str(), sql.size());
	close(fd);

	std::string cmd = duckdb_binary + " < " + tmpname + " 2>/dev/null";
	FILE *pipe = popen(cmd.c_str(), "r"); // NOLINT: intentional popen usage
	std::string output;
	if (pipe) {
		char buf[256];
		while (fgets(buf, sizeof(buf), pipe) != nullptr) {
			output += buf;
		}
		pclose(pipe);
	}
	unlink(tmpname);
	return output;
}
#endif // _WIN32

TEST_CASE("CLI: bail_on_error defaults to true for non-interactive stdin", "[shell_bail]") {
#ifndef _WIN32
	std::string duckdb_binary = std::string(DUCKDB_BUILD_DIRECTORY) + "/duckdb";
	if (access(duckdb_binary.c_str(), X_OK) != 0) {
		// duckdb binary not present in this build; skip gracefully.
		SUCCEED("duckdb binary not found at " + duckdb_binary + ", skipping test");
		return;
	}

	// Three-statement script fed through a pipe (non-interactive stdin):
	//   1. SELECT 1            – succeeds
	//   2. INVALID SQL         – fails (error)
	//   3. SELECT 'BAIL_TEST_MARKER' – must NOT execute when bail_on_error=true
	//
	// Before the fix: bail_on_error defaults to false for non-interactive stdin,
	//   so all three statements run and "BAIL_TEST_MARKER" appears in stdout.
	// After the fix:  bail_on_error defaults to true for non-interactive stdin,
	//   so processing stops after statement 2 and "BAIL_TEST_MARKER" never runs.
	std::string sql = "SELECT 1;\n"
	                  "INVALID SQL STATEMENT;\n"
	                  "SELECT 'BAIL_TEST_MARKER';\n";

	std::string output = RunDuckDBWithInput(sql);

	// Sanity: the first SELECT should have produced some output.
	REQUIRE(!output.empty());

	// The third SELECT must NOT have executed.
	REQUIRE(output.find("BAIL_TEST_MARKER") == std::string::npos);
#else
	SUCCEED("CLI bail_on_error test skipped on Windows");
#endif
}
