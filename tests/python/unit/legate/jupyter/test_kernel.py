# SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES.
#                         All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.

from __future__ import annotations

import json
from dataclasses import asdict
from typing import TYPE_CHECKING

import legate.jupyter.kernel as m
from legate.driver import LegateDriver
from legate.jupyter.config import Config
from legate.util.system import System

if TYPE_CHECKING:
    from pytest_mock import MockerFixture

    from ...util import Capsys


def test_LEGATE_JUPYTER_KERNEL_SPEC_KEY() -> None:
    assert m.LEGATE_JUPYTER_KERNEL_SPEC_KEY == "__LEGATE_JUPYTER_KERNEL_SPEC__"


def test_LEGATE_JUPYTER_METADATA_KEY() -> None:
    assert m.LEGATE_JUPYTER_METADATA_KEY == "legate"


system = System()


class Test_generate_kernel_spec:
    def test_default(self) -> None:
        config = Config([])
        driver = LegateDriver(config, system)

        spec = m.generate_kernel_spec(driver, config)

        expected_env = {
            k: v for k, v in driver.env.items() if k in driver.custom_env_vars
        }
        expected_env[m.LEGATE_JUPYTER_KERNEL_SPEC_KEY] = (
            config.kernel.spec_name
        )

        assert spec.display_name == config.kernel.display_name
        assert spec.language == "python"  # type: ignore[attr-defined]
        assert spec.argv[:-3] == list(driver.cmd)  # type: ignore[attr-defined]
        assert spec.argv[-3].endswith("_legion_kernel.py")  # type: ignore[attr-defined]
        assert spec.argv[-2:] == ["-f", "{connection_file}"]  # type: ignore[attr-defined]
        assert spec.env == expected_env  # type: ignore[attr-defined]
        assert m.LEGATE_JUPYTER_METADATA_KEY in spec.metadata
        metadata = spec.metadata[m.LEGATE_JUPYTER_METADATA_KEY]
        assert metadata == {
            "argv": config.argv[1:],
            "multi_node": asdict(config.multi_node),
            "memory": asdict(config.memory),
            "core": asdict(config.core),
        }


class Test_install_kernel_spec:
    def test_install(self, mocker: MockerFixture, capsys: Capsys) -> None:
        install_mock = mocker.patch(
            "jupyter_client.kernelspec.KernelSpecManager.install_kernel_spec"
        )

        config = Config(
            ["legate-jupyter", "--name", "____fake_test_kernel_123abc_____"]
        )
        driver = LegateDriver(config, system)

        spec = m.generate_kernel_spec(driver, config)

        m.install_kernel_spec(spec, config)

        assert install_mock.call_count == 1
        assert install_mock.call_args[0][1] == config.kernel.spec_name
        assert install_mock.call_args[1] == {
            "user": config.kernel.user,
            "prefix": config.kernel.prefix,
        }

        out, _ = capsys.readouterr()
        assert out == (
            f"Jupyter kernel spec {config.kernel.spec_name} "
            f"({config.kernel.display_name}) "
            "has been installed\n"
        )

    def test_install_verbose(
        self, mocker: MockerFixture, capsys: Capsys
    ) -> None:
        install_mock = mocker.patch(
            "jupyter_client.kernelspec.KernelSpecManager.install_kernel_spec"
        )

        config = Config(
            [
                "legate-jupyter",
                "-v",
                "--name",
                "____fake_test_kernel_123abc_____",
            ]
        )
        driver = LegateDriver(config, system)

        spec = m.generate_kernel_spec(driver, config)

        m.install_kernel_spec(spec, config)

        assert install_mock.call_count == 1
        assert install_mock.call_args[0][1] == config.kernel.spec_name
        assert install_mock.call_args[1] == {
            "user": config.kernel.user,
            "prefix": config.kernel.prefix,
        }

        out, _ = capsys.readouterr()
        assert out == (
            f"Wrote kernel spec file {config.kernel.spec_name}/kernel.json\n\n"
            f"Jupyter kernel spec {config.kernel.spec_name} "
            f"({config.kernel.display_name}) "
            "has been installed\n"
        )

    def test_install_verbose2(
        self, mocker: MockerFixture, capsys: Capsys
    ) -> None:
        install_mock = mocker.patch(
            "jupyter_client.kernelspec.KernelSpecManager.install_kernel_spec"
        )

        config = Config(
            [
                "legate-jupyter",
                "-vv",
                "--name",
                "____fake_test_kernel_123abc_____",
            ]
        )
        driver = LegateDriver(config, system)

        spec = m.generate_kernel_spec(driver, config)

        m.install_kernel_spec(spec, config)

        assert install_mock.call_count == 1
        assert install_mock.call_args[0][1] == config.kernel.spec_name
        assert install_mock.call_args[1] == {
            "user": config.kernel.user,
            "prefix": config.kernel.prefix,
        }

        out, _ = capsys.readouterr()
        spec_json = json.dumps(spec.to_dict(), sort_keys=True, indent=2)
        assert out == (
            f"Wrote kernel spec file {config.kernel.spec_name}/kernel.json\n\n"
            f"\n{spec_json}\n\n"
            f"Jupyter kernel spec {config.kernel.spec_name} "
            f"({config.kernel.display_name}) "
            "has been installed\n"
        )
