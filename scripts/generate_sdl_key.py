import pathlib

script_dir = pathlib.Path(__file__).absolute().parent


def generate_c():
    items = []
    text = (script_dir / "SDL_keycode.txt").read_text()
    for line in text.splitlines():
        if not line.startswith("#define"):
            continue
        parts = line.strip().split()
        name = parts[1]
        keyname = "key_" + underscore_to_lower_camelcase(name[len("SDLK_") :])
        s = f"{{.name = \"{keyname}\", .value = {name}}},"
        items.append(s)
    c_template = f"""
    #include <stdint.h>
    #include <SDL3/SDL_keycode.h>
    typedef struct {{
        const char *name;
        uint32_t value;
    }} Keycode;

    static const Keycode keycodes[] = {{
        {"\n".join(items)}
    }};
    """
    c_file = script_dir.parent / "dai" / "dai_canvas_keycode.h"
    c_file.write_text(c_template)


def underscore_to_lower_camelcase(name):
    parts = name.split("_")
    return parts[0].lower() + "".join(part.capitalize() for part in parts[1:])


def generate_dai():
    items = []
    text = (script_dir / "SDL_keycode.txt").read_text()
    for line in text.splitlines():
        if not line.startswith("#define"):
            continue
        parts = line.strip().split()
        name = parts[1][len("SDLK_") :]
        value = parts[2][:-1]  # 移除末尾的 u
        name = "key_" + underscore_to_lower_camelcase(name)
        s = f"    class con {name} = {value};"
        items.append(s)
    dai_template = f"""
class Keycode {{
{"\n".join(items)}
}}

class MouseButton {{
    class con left = 0;
    class con middle = 1;
    class con right = 2;
}}

class EventType {{
    class con keyDown = 0x300;
    class con keyUp = 0x301;
    class con mouseMotion = 0x400;
    class con mouseDown = 0x401;
    class con mouseUp = 0x402;
}}
    """
    dai_file = script_dir.parent / "examples" / "canvas_enum.dai"
    dai_file.write_text(dai_template)


def main():
    generate_c()
    # generate_dai()


if __name__ == "__main__":
    main()
