# Cairo Rings Fix for Conky 

## Problem

Conky rings/gauges not displaying despite Conky being compiled with Cairo bindings. The script runs without errors, but no visual rings appear on the desktop.

**Symptoms:**
- Text displays correctly
- No Lua errors in console output
- Rings/gauges completely missing

## Diagnosis

The issue is that `cairo_xlib_surface_create()` function is not available in the default Lua environment, even though Conky is compiled with Cairo support.

**Why it happens:**
- Conky provides many Cairo functions globally in its Lua environment
- However, X11-specific functions like `cairo_xlib_surface_create()` are in the `cairo_xlib` module
- This module must be explicitly loaded via `require()`

**Verification:**
```bash
# Check that Conky has Cairo support
conky -v | grep "Lua bindings"
# Output should include: Cairo
```

## Solution

Add the following lines at the beginning of your `conky_main()` function in your Lua rings script (e.g., `rings-v2_gen.lua`):

```lua
function conky_main()
    -- Load cairo and cairo_xlib modules for conky
    require "cairo"
    require "cairo_xlib"

    -- rest of your function...
    temp_watch()
    disk_watch()
    -- ...
end
```

**Key points:**
- `require "cairo"` - Loads base Cairo functions
- `require "cairo_xlib"` - Loads X11-specific functions (including `cairo_xlib_surface_create`)
- Both must be called before using any Cairo functions

## How It Works

Conky's `conky_window` table provides the X11 window information needed to create a Cairo surface:

```lua
local cs = cairo_xlib_surface_create(
    conky_window.display,   -- X11 Display
    conky_window.drawable,   -- X11 Drawable
    conky_window.visual,    -- X11 Visual
    conky_window.width,     -- Width in pixels
    conky_window.height     -- Height in pixels
)

local cr = cairo_create(cs)  -- Create Cairo context from surface

-- Your drawing code here...

-- Cleanup
cairo_destroy(cr)
cairo_surface_destroy(cs)
```

## Testing

Create a simple test script to verify Cairo is working:

**test_cairo.lua:**
```lua
function conky_test()
    require "cairo"
    require "cairo_xlib"

    if conky_window == nil then return end

    local w, h = conky_window.width, conky_window.height
    local cs = cairo_xlib_surface_create(conky_window.display,
                                         conky_window.drawable,
                                         conky_window.visual,
                                         w, h)
    local cr = cairo_create(cs)

    -- Draw a simple red circle
    cairo_set_source_rgba(cr, 1, 0, 0, 1)
    cairo_arc(cr, w/2, h/2, 50, 0, 6.283)
    cairo_fill(cr)

    cairo_surface_destroy(cs)
    cairo_destroy(cr)
end
```

**test_cairo.conkyrc:**
```lua
conky.config = {
    update_interval = 1,
    lua_load = '/path/to/test_cairo.lua',
    lua_draw_hook_pre = 'conky_test',
};

conky.text = [[]];
```

Run it:
```bash
killall conky 2>/dev/null
conky -c /path/to/test_cairo.conkyrc
```

You should see a red circle on your desktop.

## System Information

- **OS**: Debian GNU/Linux 13 (trixie)
- **Conky version**: 1.22.1
- **Desktop**: XFCE (works on any X11 desktop)
- **Lua version**: 5.3 (embedded in Conky)

## Common Mistakes

### ❌ Don't use oocairo
While `luarocks install oocairo` works, it:
- Uses an object-oriented API (`cr:set_source_rgba()` instead of `cairo_set_source_rgba()`)
- Is typically built for Lua 5.1, while Conky uses Lua 5.3
- Requires rewriting your entire rings script

### ❌ Don't install external lua-cairo packages
Debian's Conky packages include built-in Cairo bindings. You don't need:
- `lua5.4-cairo`
- `libcairo2-dev`
- Any external Lua-Cairo bridge

### ✅ Use Conky's built-in modules
Conky has Cairo compiled in. Just load the modules:
```lua
require "cairo"
require "cairo_xlib"
```

## FAQ

**Q: Why does `require "cairo"` return a string instead of a table?**  
A: Conky's `require "cairo"` returns the path to the Cairo library (`/usr/lib/conky/libcairo.so`). The actual Cairo functions are loaded into the global namespace by Conky.

**Q: Do I need to install luarocks or oocairo?**  
A: No. Conky has all the Cairo functionality you need built-in.

**Q: Will this work on other Debian versions or distros?**  
A: Yes, this solution works on any system running Conky 1.20+ with Cairo bindings compiled in.

**Q: What if I still don't see the rings?**  
A: Check that your `conky_window` properties are valid and ensure you're calling the functions in the correct hook (usually `lua_draw_hook_pre`).

## Additional Resources

- [Conky Documentation](https://conky.cc/)
- [Conky Lua API](https://conky.cc/lua)
- [Conky GitHub Repository](https://github.com/brndnmtthws/conky)
