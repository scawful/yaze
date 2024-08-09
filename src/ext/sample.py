import yaze


class MyExtension:
    def __init__(self):
        self.name = "My Python Extension"
        self.version = "1.0"

    def initialize(self):
        print(f"{self.name} Initialized")

    def cleanup(self):
        print(f"{self.name} Cleaned Up")

    def render_ui(self, editor_context):
        import imgui
        imgui.begin("My Python Extension Window")
        imgui.text("Hello from My Python Extension!")
        imgui.end()

    def manipulate_rom(self, rom):
        if rom and rom.data:
            print(f"First byte of ROM: 0x{rom.data[0]:02X}")
        else:
            print("ROM data is not loaded.")


def get_yaze_extension():
    return MyExtension()
