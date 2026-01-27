from pydantic import BaseModel, Field, field_validator
from typing import Literal


# Runtime accessible list of supported architectures
# These keys are derived from the possible values returned from QSysInfo::currentCpuArchitecture()
SUPPORTED_ARCHITECTURES = ("x86_64", "arm64", "s390x", "power64le")
ImageArch = Literal[*SUPPORTED_ARCHITECTURES]


class ImageItem(BaseModel, extra="forbid"):
    """
    Schema for an individual architecture's image metadata.
    """

    image_location: str = Field(..., description="URL to the image file")
    id: str = Field(..., description="Hash/checksum of the image")
    version: str = Field(..., description="Version string of the image")
    size: int = Field(..., description="Size of the image in bytes")


class ScraperResult(BaseModel, extra="forbid"):
    """
    Schema for the complete scraper output.
    """

    aliases: str = Field(..., description="Comma-separated list of aliases")
    os: str = Field(..., description="Operating system name")
    release: str = Field(..., description="Release identifier")
    release_codename: str = Field(..., description="Human-readable release codename")
    release_title: str = Field(..., description="Release title or version")
    items: dict[ImageArch, ImageItem] = Field(
        ..., description="Map of architecture labels to image metadata"
    )
