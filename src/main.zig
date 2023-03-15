const std = @import("std");
const getopt = @import("getopt");
const ice = @cImport({@cInclude("ib_device.h");});

const Param = struct {
  deviceId      : [:0]u8 = undefined,
  clientMac     : [:0]u8 = undefined,
  clientIp      : [:0]u8 = undefined,
  clientPort    : i32 = 10000,
  serverMac     : [:0]u8 = undefined,
  serverIp      : [:0]u8 = undefined,
  serverPort    : i32 = 10013,
  iters         : i32 = 5000,
  txQueueSize   : i32 = 128,
  rxQueueSize   : i32 = 128,
  payloadSize   : i32 = 32,
  useHugePages  : bool = false,
  isServer      : bool = false,

  // PRIVATE ACCESSORS
  fn internalPrint(object: *Param) !void {
    const stdout = std.io.getStdOut().writer();

    try std.fmt.format(stdout, "[\n", .{});
    try std.fmt.format(stdout, "  deviceId    : {s}\n", .{object.deviceId});
    try std.fmt.format(stdout, "  clientMac   : {s}\n", .{object.clientMac});
    try std.fmt.format(stdout, "  clientIp    : {s}\n", .{object.clientIp});
    try std.fmt.format(stdout, "  clientPort  : {d}\n", .{object.clientPort});
    try std.fmt.format(stdout, "  serverMac   : {s}\n", .{object.serverMac});
    try std.fmt.format(stdout, "  serverIp    : {s}\n", .{object.serverIp});
    try std.fmt.format(stdout, "  serverPort  : {d}\n", .{object.serverPort});
    try std.fmt.format(stdout, "  iters       : {d}\n", .{object.iters});
    try std.fmt.format(stdout, "  txQueueSize : {d}\n", .{object.txQueueSize});
    try std.fmt.format(stdout, "  rxQueueSize : {d}\n", .{object.rxQueueSize});
    try std.fmt.format(stdout, "  payloadSize : {d} bytes\n", .{object.payloadSize});
    try std.fmt.format(stdout, "  useHugePages: {any}\n", .{object.useHugePages});
    try std.fmt.format(stdout, "  isServer    : {any}\n", .{object.isServer});
    try std.fmt.format(stdout, "]\n", .{});
  }

  // ACCESSORS
  pub fn print(object: *Param) void {
    object.internalPrint() catch unreachable;
  }
};

pub fn usageAndExit(param: *Param) void {
  std.debug.print("Benchmark sending and receiving ethernet IPV4 UDP packets over Infiniband API.\n\n", .{});
  std.debug.print("usage: perftest ...options...\n\n", .{});
  std.debug.print("-d <string>      required: infiniband device name from 'ibstats'\n", .{});
  std.debug.print("-B <string>      required: client ethernet MAC address (ex. 08:c0:eb:d4:ec:07)\n", .{});
  std.debug.print("-j <string>      required: client IPV4 address (ex. 192.168.0.2)\n", .{});
  std.debug.print("-E <string>      required: server ethernet MAC address (ex. 08:c0:eb:d4:ec:07)\n", .{});
  std.debug.print("-J <string>      required: server IPV4 address (ex. 192.168.0.2)\n", .{});

  std.debug.print("-K <int>         optional: server IPV4 port (default {d}) in [1025,65535)\n", .{param.clientPort});
  std.debug.print("-k <int>         optional: client IPV4 port (default {d}) in [1025,65535)\n", .{param.serverPort});
  std.debug.print("-n <int>         optional: number of packets >0 to send (default {d})\n", .{param.iters});
  std.debug.print("-t <int>         optional: number of client TX request items (default {d}) in [1,4096]\n", .{param.txQueueSize});
  std.debug.print("-r <int>         optional: number of server RX request items (default {d}) in [1,4096]\n", .{param.rxQueueSize});
  std.debug.print("-s <int>         optional: size of packet payload (default {d}) in [32, 65536]\n", .{param.payloadSize});

  std.debug.print("-H               optional: allocate memory from 2048KB hugepage memory\n", .{});
  std.debug.print("-S               optional: run in server mode and client model if omitted\n", .{});
  std.debug.print("-h               optional: show usage and exit\n", .{});
  std.os.exit(2);
}

pub fn parseCommandLine(allocator: std.mem.Allocator, param: *Param) i32 {
  var valid = true;
  var intArg: i32 = undefined;
  var argCopy: [:0]u8 = undefined; 
  var arg: []const u8 = undefined;
  var opts = getopt.getopt("d:B:j:k:E:J:K:n:t:r:s:HSh");

  while (opts.next()) |maybe_opt| {
    if (maybe_opt) |opt| {
      arg = opt.arg.?;
      switch (opt.opt) {
        'd', 'B', 'j', 'E', 'J' => {
          if (allocator.dupeZ(u8, arg)) |ptr| {
            argCopy = ptr;
          } else |err| switch(err) {
            error.OutOfMemory => {
              std.debug.panic("out of memory\n", .{});
            },
          }
          switch (opt.opt) {
            'd' => {
              param.deviceId = argCopy;
            },
            'B' => {
              param.clientMac = argCopy;
            },
            'j' => {
              param.clientIp = argCopy;
            },
            'E' => {
              param.serverMac = argCopy;
            },
            'J' => {
              param.serverIp = argCopy;
            },
            else => {
              unreachable;
            }
          }
        },
        'k', 'K', 'n', 't', 'r', 's' => {
          if (std.fmt.parseInt(i32, arg, 10)) |val| {
            intArg = val;
          } else |err| switch (err) {
            std.fmt.ParseIntError.InvalidCharacter => {
              std.debug.panic("invalid character integer: '{any}'\n", .{arg});
              usageAndExit(param);
            },
            std.fmt.ParseIntError.Overflow => {
              std.debug.panic("integer overflow: '{any}'\n", .{arg});
              usageAndExit(param);
            },
          }
          switch (opt.opt) {
            'k' => {
              param.clientPort = intArg;
            },
            'K' => {
              param.serverPort = intArg;
            },
            'n' => {
              param.iters = intArg;
            },
            't' => {
              param.txQueueSize = intArg;
            },
            'r' => {
              param.rxQueueSize = intArg;
            },
            's' => {
              param.payloadSize = intArg;
            },
            else => {
              unreachable;
            }
          }
        },
        'S' => {
          param.isServer = true;
        },
        'H' => {
          param.useHugePages = true;
        },
        'h' => {
          usageAndExit(param);
        },
        else => {
          usageAndExit(param);
        },
      }
    } else {
      break;
    }
  } else |err| {
    switch (err) {
      getopt.Error.InvalidOption    => usageAndExit(param),
      getopt.Error.MissingArgument  => usageAndExit(param),
    }
    valid = false;
  }

  if (param.deviceId.len==0) {
    valid = false;
  }
  if (param.clientMac.len != 17) {
    valid = false;
  }
  if (param.serverMac.len != 17) {
    valid = false;
  }
  if (param.clientIp.len<7 or param.clientIp.len>15) {
    valid = false;
  }
  if (param.serverIp.len<7 or param.serverIp.len>15) {
    valid = false;
  }
  if (param.clientPort<1025 or param.clientPort>65535) {
    valid = false;
  }
  if (param.serverPort<1025 or param.serverPort>65535) {
    valid = false;
  }
  if (param.iters<=0) {
    valid = false;
  }
  if (param.txQueueSize<1 or param.txQueueSize>4096) {
    valid = false;
  }
  if (param.rxQueueSize<1 or param.rxQueueSize>4096) {
    valid = false;
  }
  if (param.payloadSize<32 or param.payloadSize>65536) {
    valid = false;
  }

  if (!valid) {
    usageAndExit(param);
  }

  return 0;
}

pub fn main() u8 {
  var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
  defer arena.deinit();

  var param = Param{};
  if (0!=parseCommandLine(arena.allocator(), &param)) {
    return 2;
  }

  param.print();

  std.log.debug("binding to '{s}' ...\n", .{param.deviceId});
  _ = ice.ice_ib_find_device(param.deviceId.ptr);

  return 0;
}
