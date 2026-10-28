import struct
import renderdoc as rd
import rdtest


class VK_Untyped_Pointers(rdtest.TestCase):
    demos_test_name = 'VK_Untyped_Pointers'

    def check_capture(self):
        action = self.find_action('Draw')
        assert action is not None
        action = self.find_action('vkCmdDraw', action.eventId)
        assert action is not None

        self.set_event(action.eventId, True)

        pipe = self.controller.GetPipelineState()

        # tint * reinterpreted scale + bufarray[2].col
        expected = [0.5, 0.75, 0.75, 1.0]

        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 0.5, 0.5, expected, eps=1.0/255.0)

        rdtest.log.success('Triangle is rendered correctly')

        # check the reflection of the untyped variables
        vsrefl = pipe.GetShaderReflection(rd.ShaderStage.Vertex)
        fsrefl = pipe.GetShaderReflection(rd.ShaderStage.Fragment)

        self.check_eq([r.name for r in vsrefl.readWriteResources], ['vssbo'])
        self.check_eq(vsrefl.readWriteResources[0].fixedBindNumber, 1)
        self.check_eq([m.name for m in vsrefl.readWriteResources[0].variableType.members], ['words'])

        self.check_eq(sorted([c.name for c in fsrefl.constantBlocks]), ['push', 'ubo'])
        self.check_eq(sorted([r.name for r in fsrefl.readWriteResources]), ['bufarray', 'outbuf', 'ssbo'])

        ubo_idx = [c.name for c in fsrefl.constantBlocks].index('ubo')
        push_idx = [c.name for c in fsrefl.constantBlocks].index('push')

        ubo = fsrefl.constantBlocks[ubo_idx]
        self.check_eq(ubo.bufferBacked, True)
        self.check_eq(ubo.fixedBindNumber, 0)
        self.check_eq([v.name for v in ubo.variables], ['tint', 'unused'])

        push = fsrefl.constantBlocks[push_idx]
        self.check_eq(push.bufferBacked, False)
        self.check_eq([v.name for v in push.variables], ['index'])

        bufarray = [r for r in fsrefl.readWriteResources if r.name == 'bufarray'][0]
        self.check_eq(bufarray.fixedBindNumber, 3)
        self.check_eq(bufarray.bindArraySize, 4)
        self.check_eq([m.name for m in bufarray.variableType.members], ['col'])

        outbuf = [r for r in fsrefl.readWriteResources if r.name == 'outbuf'][0]
        self.check_eq([m.name for m in outbuf.variableType.members], ['firstword', 'result'])
        self.check_eq(outbuf.variableType.members[1].byteOffset, 16)

        rdtest.log.success('Untyped variables are reflected correctly')

        # check the constant buffer contents
        cbuf = pipe.GetConstantBlock(rd.ShaderStage.Fragment, ubo_idx, 0).descriptor
        var_check = rdtest.ConstantBufferChecker(
            self.controller.GetCBufferVariableContents(pipe.GetGraphicsPipelineObject(),
                                                       pipe.GetShader(rd.ShaderStage.Fragment),
                                                       rd.ShaderStage.Fragment, fsrefl.entryPoint, ubo_idx,
                                                       cbuf.resource, cbuf.byteOffset, cbuf.byteSize))
        var_check.check('tint').type(rd.VarType.Float).rows(1).cols(4).value([0.5, 0.25, 0.75, 1.0])
        var_check.check('unused').type(rd.VarType.Float).rows(1).cols(4).value([0.0, 0.0, 0.0, 0.0])
        var_check.done()

        cbuf = pipe.GetConstantBlock(rd.ShaderStage.Fragment, push_idx, 0).descriptor
        var_check = rdtest.ConstantBufferChecker(
            self.controller.GetCBufferVariableContents(pipe.GetGraphicsPipelineObject(),
                                                       pipe.GetShader(rd.ShaderStage.Fragment),
                                                       rd.ShaderStage.Fragment, fsrefl.entryPoint, push_idx,
                                                       cbuf.resource, cbuf.byteOffset, cbuf.byteSize))
        var_check.check('index').type(rd.VarType.UInt).rows(1).cols(1).value([2])
        var_check.done()

        rdtest.log.success('Constant blocks have the correct contents')

        # the dynamically indexed descriptor array should only report the element that was used
        bufarray_idx = list(fsrefl.readWriteResources).index(bufarray)
        used = [a.access.arrayElement for a in pipe.GetReadWriteResources(rd.ShaderStage.Fragment, True)
                if a.access.index == bufarray_idx]
        self.check_eq(used, [2])

        rdtest.log.success('Descriptor array access is tracked correctly')

        # check the output written through untyped pointers
        for a in pipe.GetReadWriteResources(rd.ShaderStage.Fragment):
            if fsrefl.readWriteResources[a.access.index].name != 'outbuf':
                continue
            data = self.controller.GetBufferData(a.descriptor.resource, a.descriptor.byteOffset, 32)
            firstword = struct.unpack_from('I', data, 0)[0]
            result = struct.unpack_from('4f', data, 16)

            self.check_eq(firstword, 0x12345678)
            if not rdtest.value_compare(result, expected):
                raise rdtest.TestFailureException(f'Output buffer result {result} is not as expected {expected}')

        rdtest.log.success('Output buffer has the correct contents')

        # the vertex shader reads its position scale through an untyped pointer, so if post-VS
        # patching broke the shader the positions would be wrong
        postvs_ref: rdtest.MeshReference = {
            0: {
                'vtx': 0,
                'idx': 0,
                '_Position': [-0.5, -0.5, 0.0, 1.0],
                'vertOut': [0.0, 1.0, 0.0, 1.0],
            },
            1: {
                'vtx': 1,
                'idx': 1,
                '_Position': [0.0, 0.5, 0.0, 1.0],
                'vertOut': [0.0, 1.0, 0.0, 1.0],
            },
            2: {
                'vtx': 2,
                'idx': 2,
                '_Position': [0.5, -0.5, 0.0, 1.0],
                'vertOut': [0.0, 1.0, 0.0, 1.0],
            },
        }

        postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut, 0, action.numIndices)
        self.check_mesh_data(postvs_ref, postvs_data)

        rdtest.log.success('Mesh output data is correct')

        # disassembly should succeed and show the untyped variables
        disasm = self.controller.DisassembleShader(pipe.GetGraphicsPipelineObject(), fsrefl, '')
        if 'untyped' not in disasm:
            raise rdtest.TestFailureException('Disassembly does not contain untyped variable declarations')

        rdtest.log.success('Disassembly is generated')

        # shader debugging isn't supported with untyped pointers, it should be cleanly refused
        self.check_eq(vsrefl.debugInfo.debuggable, False)
        self.check_eq(fsrefl.debugInfo.debuggable, False)

        rdtest.log.success('Shaders are reported as not debuggable')

        # pixel history patches the fragment shader to strip side effects, including stores through
        # untyped pointers
        tex = pipe.GetOutputTargets()[0].resource
        tex_details = self.get_texture(tex)
        x = tex_details.width // 2
        y = tex_details.height // 2

        with self.pixel_history(tex, x, y, rd.Subresource(0, 0, 0), rd.CompType.Typeless) as history:
            modifs = [m for m in history.modifs if m.eventId == action.eventId]

            if len(modifs) != 1:
                raise rdtest.TestFailureException(f'Expected one modification from the draw, got {len(modifs)}')

            if not modifs[0].Passed():
                raise rdtest.TestFailureException('Draw modification did not pass')

            if not rdtest.value_compare(modifs[0].shaderOut.col.floatValue, expected):
                raise rdtest.TestFailureException(
                    f'Pixel history shader output {modifs[0].shaderOut.col.floatValue} is not as expected {expected}')

        rdtest.log.success('Pixel history is correct')
