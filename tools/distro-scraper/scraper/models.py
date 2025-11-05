from pydantic import BaseModel, Field, field_validator


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
    items: dict[str, ImageItem] = Field(
        ..., description="Map of architecture labels to image metadata"
    )

    @field_validator("items")
    @classmethod
    def validate_architecture_keys(
        cls, v: dict[str, ImageItem]
    ) -> dict[str, ImageItem]:
        """
        Ensure items dict only contains valid architecture keys.
        """
        allowed_keys = {"x86_64", "arm64"}
        invalid_keys = set(v.keys()) - allowed_keys
        if invalid_keys:
            raise ValueError(
                f"Invalid architecture keys: {invalid_keys}. "
                f"Only {allowed_keys} are allowed."
            )
        return v
