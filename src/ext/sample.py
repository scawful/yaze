import yaze

class YazePyExtension:
    def __init__(self):
        self.name = "My Python Extension"
        self.version = "1.0"

    def initialize(self, context):
        print(f"{self.name} Initialized with context: {context}")
        self.context = context
        self.register_event_hooks()

    def cleanup(self):
        print(f"{self.name} Cleaned Up")

    def manipulate_rom(self, rom):
        if rom and rom.data:
            print(f"First byte of ROM: 0x{rom.data[0]:02X}")
        else:
            print("ROM data is not loaded.")

    def extend_ui(self, context):
        import imgui
        if imgui.begin("My Python Extension Window"):
            imgui.text("Hello from My Python Extension!")
            imgui.end()

    def register_commands(self):
        # Register custom commands here
        print(f"{self.name} Commands Registered")

    def register_custom_tools(self):
        # Register custom tools here
        print(f"{self.name} Custom Tools Registered")

    def on_rom_loaded(self):
        print("ROM has been loaded!")

    def register_event_hooks(self):
        # Register event hooks, like for ROM loaded
        self.context.register_event_hooks(
            YAZE_EVENT_ROM_LOADED, self.on_rom_loaded)


def get_yaze_extension():
    return YazePyExtension()
