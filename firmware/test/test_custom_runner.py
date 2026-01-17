import click

from platformio.public import UnityTestRunner

class CustomTestRunner(UnityTestRunner):
    """This custom unity testrunner will output verbose info."""
    #https://github.com/platformio/platformio-core/issues/4276#issuecomment-1145840723

    def on_testing_line_output(self, line):
        click.echo(line, nl=False)
        super().on_testing_line_output(line)
        