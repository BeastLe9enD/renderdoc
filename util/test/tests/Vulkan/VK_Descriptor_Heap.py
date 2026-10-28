import renderdoc as rd
import rdtest


class VK_Descriptor_Heap(rdtest.TestCase):
    demos_test_name = 'VK_Descriptor_Heap'

    def check_capture(self):
        # the first draw samples a green texture with a white UBO and no tint, the second samples a
        # white texture with a blue UBO and a red tint pushed via push data
        expected = {
            'Set 0': [0.0, 1.0, 0.0, 1.0],
            'Set 1': [1.0, 0.0, 1.0, 1.0],
        }

        for marker, value in expected.items():
            rdtest.log.begin_section(marker)

            draw = self.find_action(marker)

            if draw is None:
                raise rdtest.TestFailureException(f"Couldn't find '{marker}' marker")

            self.set_event(draw.nextAction.eventId)

            pipe = self.controller.GetPipelineState()

            v = pipe.GetViewport(0)

            x = int(v.x) + int(v.width / 2)
            y = int(v.y) + int(v.height / 2)

            out = pipe.GetOutputTargets()[0].resource

            self.check_pixel_value(out, x, y, value)

            rdtest.log.success(f"Draw at '{marker}' has the correct output")

            rdtest.log.end_section(marker)
