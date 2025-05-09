# AWS Greengrass SDK Lite

`aws-greengrass-sdk-lite` provides an API for calling AWS IoT Greengrass IPC
calls with a small footprint. It enables Greengrass components to interact with
the Greengrass Nucleus (Lite or Classic) with less binary overhead, and also
enables components written in C. This can be used by Greengrass components as an
alternative to the `aws-iot-device-sdk-cpp-v2` or the other language device
SDKs.

### ⚠️ Important Notice ⚠️

This library is pre-release. Feel free to test it out and report bugs or missing
features.

Usage for development should pin a commit or tag.

## Security

See [CONTRIBUTING](docs/CONTRIBUTING.md#security-issue-notifications) for more
information.

## License

This project is licensed under the Apache-2.0 License.
