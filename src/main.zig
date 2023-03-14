const std = @import("std");
const clap = @import("clap");

pub fn parseCommandLine() i32 {
  const params = comptime clap.parseParamsCompTime(
    \\-d, --device          required: string: infiniband device name from 'ibstats'
    \\-B, --clientMac       required: string: client (data transmitter) eth MAC address (ex. 08:c0:eb:d4:ec:07)
    \\-j, --clientIp        required: string: client IPV4 address (ex. 192.168.0.2)
    \\-k, --clientPort      required: int   : client IPV4 port in [1025,65535)
    \\-E, --serverMac       required: string: server (data receiver) eth MAC address *ex. 08:c0:eb:d4:ec:07)
    \\-J, --serverIp        required: string: server IPV4 address (ex. 192.168.0.2)
    \\-K, --serverPort      required: int   : server IPV4 port in [1024,65535])
    \\-n, --iters           required: int   : number of packets to send
    \\-H, --hugepages       optional:       : allocate memory from 2048Kb hugepages
    \\-S, --server          optional:       : run in server mode (otherwise client)
    \\-t, --clientTxSize    optional: int   : number of client TX request items in [1,4096]   
    \\-r, --serverRxSize    optional: int   : number of server RX request items in [1,4096]   
    \\-s, --packetSize      optional: int   : total packet size in [32,65536] bytes
    \\-h, --help            optional:       : display usage and exit
    \\
  );

  var diag = clap.Diagnostic{};
  var res = clap.parse(clap.Help, &params, clap.parsers.default, .{
        .diagnostic = &diag,
    }) catch |err| {
        // Report useful error and exit
        diag.report(std.io.getStdErr().writer(), err) catch {};
        return 1;
    };
  defer res.deinit();
    
  return 0;
}

pub fn main() u8 {
  if (0!=parseCommandLine()) {
    return 1;
  }
  return 0;
}
