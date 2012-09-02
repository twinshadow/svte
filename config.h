/*
	VTE_CURSOR_BLINK_SYSTEM
	VTE_CURSOR_BLINK_ON
	VTE_CURSOR_BLINK_OFF
 */
static int cursor_blink = VTE_CURSOR_BLINK_ON;

/*
	VTE_CURSOR_SHAPE_BLOCK,
	VTE_CURSOR_SHAPE_IBEAM,
	VTE_CURSOR_SHAPE_UNDERLINE
*/
static int cursor_shape = VTE_CURSOR_BLINK_ON;


static int pty_flags = VTE_PTY_DEFAULT;

/*
 Creates a new font description from a string representation in the form
 "[FAMILY-LIST] [STYLE-OPTIONS] [SIZE]", where FAMILY-LIST is a comma separated
 list of families optionally terminated by a comma, STYLE_OPTIONS is a
 whitespace separated list of words where each WORD describes one of style,
 variant, weight, stretch, or gravity, and SIZE is a decimal number (size in
 points) or optionally followed by the unit modifier "px" for absolute size.
 Any one of the options may be absent. If FAMILY-LIST is absent, then the
 family_name field of the resulting font description will be initialized to
 NULL. If STYLE-OPTIONS is missing, then all style options will be set to the
 default values. If SIZE is missing, the size in the resulting font description
 will be set to 0.

http://developer.gnome.org/pango/stable/pango-Fonts.html#pango-font-description-from-string
 */
static const char font[] = "ProggyCleanTT 12";

static const char bgimage[] = {0};
static const char bgcolor[] = "black";
static const char fgcolor[] = "#3a3";
static const char bgtintcolor[] = "black";
static const char cursorcolor[] = "#3a3";
static const char word_chars[] = "-A-Za-z0-9_$.+!*(),;:@&=?/~#%";
static const char emulation[] = "xterm";
static const char encoding[] = {0}; //System default
static const char shell[] = "/bin/zsh";

bool	transparent = true,
	audible = false,
	dbuffer = true,
	scrollbar = false,
	icon_title = false,
	use_geometry_hints = false,
	bolding = false,
	mouse_autohide = true;

static int32_t	savelines = 16383;
static uint16_t opacity = 0xffff;

static const char *colormapping[] = {
	"#666666",
	"#ff6666",
	"#66ff66",
	"#ffff66",
	"#6666ff",
	"#ff66ff",
	"#66ffff",
	"#aaaaaa",
	"#999999",
	"#ff9999",
	"#99ff99",
	"#ffff99",
	"#9999ff",
	"#ff99ff",
	"#99ffff",
	"#dddddd",
	NULL
};

