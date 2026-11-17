import contextlib
import io
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import check_boundaries


class CheckBoundariesTest(unittest.TestCase):
    def setUp(self):
        self.temporaryDirectory = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporaryDirectory.cleanup)
        self.root = self.temporaryDirectory.name

    def write(self, relativePath, content):
        path = os.path.join(self.root, relativePath)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as handle:
            handle.write(content)

    def run_check(self, modules, games=(), plugins=()):
        manifest = {
            "modules": [{"name": name, "directory": directory, "depends": depends} for name, directory, depends in modules],
            "plugins": [{"name": name, "directory": directory, "depends": depends} for name, directory, depends in plugins],
            "games": [{"name": name, "directory": directory, "depends": depends} for name, directory, depends in games],
        }
        manifestPath = os.path.join(self.root, "manifest.json")
        with open(manifestPath, "w", encoding="utf-8") as handle:
            json.dump(manifest, handle)

        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            code = check_boundaries.main(["--root", self.root, "--manifest", manifestPath])
        return code, output.getvalue()

    def test_declared_include_passes(self):
        self.write("engine/runtime/ecs/src/x.cpp", '#include "oly/core/assert.hpp"\n')

        code, output = self.run_check([("core", "engine/runtime/core", []), ("ecs", "engine/runtime/ecs", ["core"])])

        self.assertEqual(code, 0)
        self.assertEqual(output, "")

    def test_undeclared_include_fails(self):
        self.write("engine/runtime/capi/src/x.cpp", '#include <cstdint>\n\n#include "oly/core/assert.hpp"\n')

        code, output = self.run_check([("ecs", "engine/runtime/ecs", []), ("capi", "engine/runtime/capi", ["ecs"])])

        self.assertEqual(code, 1)
        self.assertIn("engine/runtime/capi/src/x.cpp:3:", output)
        self.assertIn("without declaring core", output)

    def test_detail_from_outside_fails(self):
        self.write("engine/runtime/capi/src/x.cpp", '#include "oly/ecs/detail/table.hpp"\n')

        code, output = self.run_check([("ecs", "engine/runtime/ecs", []), ("capi", "engine/runtime/capi", ["ecs"])])

        self.assertEqual(code, 1)
        self.assertIn("oly/ecs/detail is internal to module ecs", output)

    def test_detail_in_module_tests_passes(self):
        self.write("engine/runtime/ecs/tests/t.cpp", '#include "oly/ecs/detail/table.hpp"\n')

        code, output = self.run_check([("ecs", "engine/runtime/ecs", [])])

        self.assertEqual(code, 0)
        self.assertEqual(output, "")

    def test_detail_in_environment_module_passes(self):
        self.write("engine/environments/native/host/src/x.cpp", '#include "oly/native_host/detail/input_translation.hpp"\n')
        self.write("engine/environments/native/host/tests/t.cpp", '#include "oly/native_host/detail/input_translation.hpp"\n')

        self.write("engine/environments/native/host/include/oly/native_host/detail/input_translation.hpp", "")

        code, output = self.run_check([])

        self.assertEqual(code, 0)
        self.assertEqual(output, "")

    def test_detail_of_environment_module_from_outside_fails(self):
        self.write("games/g/x.cpp", '#include "oly/native_host/detail/input_translation.hpp"\n')

        code, output = self.run_check([("native_host", "engine/environments/native/host", [])], games=[("g", "games/g", ["native_host"])])

        self.assertEqual(code, 1)
        self.assertIn("oly/native_host/detail is internal to module native_host", output)

    def test_game_uses_game_depends(self):
        self.write("engine/fixture/systems.cpp", '#include "oly/core/assert.hpp"\n')

        code, output = self.run_check(
            [("core", "engine/runtime/core", []), ("ecs", "engine/runtime/ecs", ["core"]), ("capi", "engine/runtime/capi", ["ecs"])],
            games=[("fixture_game", "engine/fixture", ["ecs", "capi"])],
        )

        self.assertEqual(code, 1)
        self.assertIn("engine/fixture/systems.cpp:1:", output)

    def test_internal_outside_engine_fails(self):
        self.write("games/x/src/a.ts", 'import { Bridge } from "@oly/core/internal";\n')

        code, output = self.run_check([])

        self.assertEqual(code, 1)
        self.assertIn("games/x/src/a.ts:1: @oly/core/internal can only be used inside engine/", output)

    def test_internal_inside_engine_passes(self):
        self.write("engine/testing/tests/a.ts", 'import { Bridge } from "@oly/core/internal";\n')

        code, output = self.run_check([])

        self.assertEqual(code, 0)
        self.assertEqual(output, "")

    def test_core_importing_web_host_fails(self):
        self.write("engine/environments/web/clients/typescript/core/src/a.ts", 'import { moduleFactoryFromUrl } from "@oly/web-host";\n')

        code, output = self.run_check([])

        self.assertEqual(code, 1)
        self.assertIn("engine/environments/web/clients/typescript/core/src/a.ts:1: @oly/core cannot depend on @oly/web-host", output)

    def test_relative_across_packages_fails(self):
        self.write("engine/environments/web/host/js/package.json", "{}")
        self.write("engine/environments/web/clients/typescript/core/package.json", "{}")
        self.write("engine/environments/web/host/js/src/a.ts", 'import { World } from "../../../clients/typescript/core/src/world.js";\n')

        code, output = self.run_check([])

        self.assertEqual(code, 1)
        self.assertIn("engine/environments/web/host/js/src/a.ts:1: relative import crosses packages; use the package name", output)

    def test_runtime_including_environment_fails(self):
        self.write("engine/environments/web/host/native/include/oly/web_host/script_invoker.hpp", "#pragma once\n")
        self.write("engine/runtime/capi/src/x.cpp", '#include "oly/web_host/script_invoker.hpp"\n')

        code, output = self.run_check([("web_host", "engine/environments/web/host/native", ["capi"]), ("capi", "engine/runtime/capi", ["web_host"])])

        self.assertEqual(code, 1)
        self.assertIn("engine/runtime/capi/src/x.cpp:1: engine/runtime cannot depend on environment web (oly/web_host)", output)

    def test_environment_including_other_environment_fails(self):
        self.write("engine/environments/native/host/include/oly/native_host/window.hpp", "#pragma once\n")
        self.write("engine/environments/web/host/native/src/x.cpp", '#include "oly/native_host/window.hpp"\n')

        code, output = self.run_check([("native_host", "engine/environments/native/host", []), ("web_host", "engine/environments/web/host/native", ["native_host"])])

        self.assertEqual(code, 1)
        self.assertIn("engine/environments/web/host/native/src/x.cpp:1: environment web cannot depend on environment native (oly/native_host)", output)

    def test_environment_importing_other_environment_package_fails(self):
        self.write("engine/environments/other/clients/x/package.json", '{"name": "@oly/other-client"}')
        self.write("engine/environments/web/clients/typescript/core/src/a.ts", 'import { thing } from "@oly/other-client";\n')

        code, output = self.run_check([])

        self.assertEqual(code, 1)
        self.assertIn("engine/environments/web/clients/typescript/core/src/a.ts:1: environment web cannot depend on environment other (@oly/other-client)", output)

    def test_emscripten_in_runtime_fails(self):
        self.write("engine/runtime/app/src/x.cpp", "#if defined(__EMSCRIPTEN__)\n#endif\n")

        code, output = self.run_check([])

        self.assertEqual(code, 1)
        self.assertIn("engine/runtime/app/src/x.cpp:1: Emscripten (__EMSCRIPTEN__) is allowed only in engine/environments/web/ and OLY_API", output)

    def test_em_macro_in_runtime_test_fails(self):
        self.write("engine/runtime/app/tests/t.cpp", "EM_ASM({});\n")

        code, output = self.run_check([])

        self.assertEqual(code, 1)
        self.assertIn("engine/runtime/app/tests/t.cpp:1: Emscripten (EM_ASM) is allowed only in engine/environments/web/ and OLY_API", output)

    def test_runtime_importing_environment_package_fails(self):
        self.write("engine/environments/web/clients/x/package.json", '{"name": "@oly/web-client"}')
        self.write("engine/runtime/app/src/a.ts", 'import { thing } from "@oly/web-client";\n')

        code, output = self.run_check([])

        self.assertEqual(code, 1)
        self.assertIn("engine/runtime/app/src/a.ts:1: engine/runtime cannot depend on environment web (@oly/web-client)", output)

    def test_emscripten_version_macro_in_runtime_fails(self):
        self.write("engine/runtime/app/src/x.cpp", "int major = __EMSCRIPTEN_major__;\n")

        code, output = self.run_check([])

        self.assertEqual(code, 1)
        self.assertIn("engine/runtime/app/src/x.cpp:1: Emscripten (__EMSCRIPTEN_major__) is allowed only in engine/environments/web/ and OLY_API", output)

    def test_emscripten_in_runtime_header_with_other_extension_fails(self):
        self.write("engine/runtime/app/include/oly/app/x.h", "#if defined(__EMSCRIPTEN__)\n#endif\n")

        code, output = self.run_check([])

        self.assertEqual(code, 1)
        self.assertIn("engine/runtime/app/include/oly/app/x.h:1: Emscripten (__EMSCRIPTEN__) is allowed only in engine/environments/web/ and OLY_API", output)

    def test_emscripten_in_runtime_c_file_fails(self):
        self.write("engine/runtime/capi/src/x.c", "#ifdef __EMSCRIPTEN__\n#endif\n")

        code, output = self.run_check([])

        self.assertEqual(code, 1)
        self.assertIn("engine/runtime/capi/src/x.c:1: Emscripten (__EMSCRIPTEN__) is allowed only in engine/environments/web/ and OLY_API", output)

    def test_wasm_skip_in_runtime_bench_passes(self):
        self.write("engine/runtime/app/bench/b.cpp", "#if !defined(__EMSCRIPTEN__)\n#endif\n")

        code, output = self.run_check([])

        self.assertEqual(code, 0)
        self.assertEqual(output, "")

    def test_emscripten_in_allowed_places_passes(self):
        self.write("engine/runtime/capi/include/oly/capi/c_api.h", "#include <emscripten.h>\n#define OLY_API EMSCRIPTEN_KEEPALIVE\n")
        self.write("engine/environments/web/host/native/src/x.cpp", "#include <emscripten.h>\nEM_JS(void, f, (), {});\n")
        self.write("engine/runtime/app/tests/t.cpp", "#if !defined(__EMSCRIPTEN__)\n#endif\n")

        code, output = self.run_check([])

        self.assertEqual(code, 0)
        self.assertEqual(output, "")

    def test_generated_subpath_in_hand_written_code_fails(self):
        self.write("games/g/web/src/a.ts", 'import { componentType } from "@oly/core/generated";\n')

        code, output = self.run_check([])

        self.assertEqual(code, 1)
        self.assertIn("games/g/web/src/a.ts:1: @oly/core/generated can only be imported by generated code (*.generated.ts)", output)

    def test_generated_subpath_in_generated_code_and_in_core_passes(self):
        self.write("games/g/web/schema.generated.ts", 'import { componentType } from "@oly/core/generated";\n')
        self.write("engine/environments/web/clients/typescript/core/tests/unit/generated.test.ts", 'import { schemaOf } from "@oly/core/generated";\n')

        code, output = self.run_check([])

        self.assertEqual(code, 0)
        self.assertEqual(output, "")

    def test_schema_tooling_including_environment_fails(self):
        self.write("engine/environments/web/clients/typescript/codegen/include/oly/schema_typescript/typescript_emitter.hpp", "#pragma once\n")
        self.write("engine/schema/codegen/src/x.cpp", '#include "oly/schema_typescript/typescript_emitter.hpp"\n')

        code, output = self.run_check([("schema", "engine/schema/codegen", ["schema_typescript"]), ("schema_typescript", "engine/environments/web/clients/typescript/codegen", ["schema"])])

        self.assertEqual(code, 1)
        self.assertIn("engine/schema/codegen/src/x.cpp:1: engine/schema cannot depend on environment web (oly/schema_typescript)", output)

    def test_engine_includes_plugin_fails(self):
        self.write("plugins/counting/include/oly/counting/plugin.hpp", "#pragma once\n")
        self.write("engine/runtime/app/src/x.cpp", '#include "oly/counting/plugin.hpp"\n')

        code, output = self.run_check([("app", "engine/runtime/app", ["counting"])], plugins=[("counting", "plugins/counting", ["app"])])

        self.assertEqual(code, 1)
        self.assertIn("engine/runtime/app/src/x.cpp:1: engine cannot depend on plugin counting", output)

    def test_plugin_includes_undeclared_fails(self):
        self.write("plugins/counting/src/plugin.cpp", '#include "oly/capi/c_api.h"\n')

        code, output = self.run_check([("app", "engine/runtime/app", []), ("capi", "engine/runtime/capi", [])], plugins=[("counting", "plugins/counting", ["app"])])

        self.assertEqual(code, 1)
        self.assertIn("plugins/counting/src/plugin.cpp:1: counting includes oly/capi without declaring capi in DEPENDS", output)

    def test_plugin_includes_other_detail_fails(self):
        self.write("plugins/counting/src/plugin.cpp", '#include "oly/ecs/detail/table.hpp"\n')

        code, output = self.run_check([("ecs", "engine/runtime/ecs", [])], plugins=[("counting", "plugins/counting", ["ecs"])])

        self.assertEqual(code, 1)
        self.assertIn("plugins/counting/src/plugin.cpp:1: oly/ecs/detail is internal to module ecs", output)

    def test_game_name_in_plugin_fails(self):
        self.write("games/arena/arena.oly", "")
        self.write("plugins/counting/src/plugin.cpp", "// tuned for arena_game\n")
        self.write("plugins/counting/arena/notes.cpp", "")

        code, output = self.run_check([], plugins=[("counting", "plugins/counting", [])])

        self.assertEqual(code, 1)
        self.assertIn("plugins/counting/src/plugin.cpp:1: game name arena in plugins/", output)
        self.assertIn("plugins/counting/arena/notes.cpp: game name arena in plugins/", output)

    def test_game_name_inside_another_word_in_plugin_passes(self):
        self.write("games/arena/arena.oly", "")
        self.write("plugins/counting/src/plugin.cpp", "// an arenas pool; see Arena\n")

        code, output = self.run_check([], plugins=[("counting", "plugins/counting", [])])

        self.assertEqual(code, 0)
        self.assertEqual(output, "")

    def test_engine_test_game_outside_engine_fails(self):
        self.write("games/g/CMakeLists.txt", "oly_add_engine_test_game(g_game ENVIRONMENT web)\n")
        self.write("engine/testing/CMakeLists.txt", "oly_add_engine_test_game(fixture_game ENVIRONMENT web)\n")

        code, output = self.run_check([])

        self.assertEqual(code, 1)
        self.assertEqual(output, "games/g/CMakeLists.txt:1: oly_add_engine_test_game is only for engine/\n")

    def test_digit_in_module_name_resolves_its_owner(self):
        self.write("plugins/essentials2d/include/oly/essentials2d/transform.hpp", "#pragma once\n")
        self.write("games/g/x.cpp", '#include "oly/essentials2d/transform.hpp"\n')
        self.write("engine/runtime/app/src/x.cpp", '#include "oly/essentials2d/transform.hpp"\n')

        code, output = self.run_check([("app", "engine/runtime/app", ["essentials2d"])], games=[("g", "games/g", [])], plugins=[("essentials2d", "plugins/essentials2d", [])])

        self.assertEqual(code, 1)
        self.assertIn("games/g/x.cpp:1: g includes oly/essentials2d without declaring essentials2d in DEPENDS", output)
        self.assertIn("engine/runtime/app/src/x.cpp:1: engine cannot depend on plugin essentials2d", output)


if __name__ == "__main__":
    unittest.main()
