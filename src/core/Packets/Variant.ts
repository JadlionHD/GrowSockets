import TankPacket from "./TankPacket";

// Types
import { Arg, Options, ArgType } from "../Types/Variant";

/**
 * Represents the Variant class.
 */
class Variant {
  public index: number = 0;

  /**
   * Creates a new instance of the Variant class.
   * @param options The options for the variant.
   * @param args The arguments of the Variant.
   */
  constructor(public options: Options = {}, public args: Arg[]) {}

  /**
   * Creates a new Variant class.
   * @param opts The options for the variant.
   * @param args The arguments of the Variant.
   */
  public static from(opts?: Options | Arg, ...args: Arg[]) {
    if (
      typeof opts === "string" ||
      typeof opts === "number" ||
      Array.isArray(opts)
    ) {
      args.unshift(opts);
      opts = { netID: -1, delay: 0 };
    }

    return new Variant(opts as Options, args);
  }

  /**
   * Parses the data of the Variant and returns a TankPacket from it.
   */
  public parse() {
    let buf = [this.args.length];

    this.args.forEach((arg) => {
      buf.push(this.index++);

      switch (typeof arg) {
        case "string": {
          buf.push(ArgType.STRING);

          const bytes = new Uint32Array(1);
          bytes[0] = arg.length;

          const uint8_buf = new Uint8ClampedArray(bytes.buffer);
          const text_buf = new TextEncoder().encode(arg);

          buf = [...buf, ...uint8_buf, ...text_buf];
          break;
        }

        case "number": {
          let bytes;

          if (arg < 0) {
            bytes = new Int32Array(1);
            buf.push(ArgType.SIGNED_INT);
          } else {
            bytes = new Uint32Array(1);
            buf.push(ArgType.UNSIGNED_INT);
          }

          bytes[0] = arg;

          const uint8_buf = new Uint8ClampedArray(bytes.buffer);
          buf = [...buf, ...uint8_buf];
          break;
        }

        case "object": {
          if (!Array.isArray(arg)) return;

          const type = ArgType[`FLOAT_${arg.length}`];
          if (!type) return;

          buf.push(type);

          arg.forEach((float) => {
            const bytes = new Float32Array(1);
            bytes[0] = float;

            const uint8_buf = new Uint8ClampedArray(bytes.buffer);
            buf = [...buf, ...uint8_buf];
          });
          break;
        }
      }
    });

    return TankPacket.from({
      type: 0x1,
      netID: this.options.netID || -1,
      info: this.options.delay || 0,
      data: () => Buffer.from(buf),
    });
  }
}

export default Variant;
