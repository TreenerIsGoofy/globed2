use std::{fmt::Display, time::SystemTimeError};

use crate::data::{types::ColorParseError, DecodeError};

pub enum PacketHandlingError {
    Other(String),                         // unknown generic error
    WrongCryptoBoxState,                   // cryptobox was either Some or None when should've been the other one
    EncryptionError,                       // failed to encrypt data
    DecryptionError,                       // failed to decrypt data
    IOError(std::io::Error),               // generic IO error
    MalformedMessage,                      // packet is missing a header
    MalformedLoginAttempt,                 // LoginPacket with cleartext credentials
    MalformedCiphertext,                   // missing nonce/mac in the encrypted ciphertext
    MalformedPacketStructure(DecodeError), // failed to decode the packet
    NoHandler(u16),                        // no handler found for this packet ID
    WebRequestError(reqwest::Error),       // error making a web request to the central server
    UnexpectedPlayerData,                  // client sent PlayerDataPacket or SyncPlayerMetadataPacket outside of a level
    SystemTimeError(SystemTimeError),      // clock went backwards..?
    SocketSendFailed(std::io::Error),      // failed to send data on a socket due to an IO error
    SocketWouldBlock,                      // failed to send data on a socket because operation would block
    UnexpectedCentralResponse,             // malformed response from the central server
    ColorParseFailed(ColorParseError),     // failed to parse a hex color into a Color3B struct
    Ratelimited,                           // too many packets per second
    DangerousAllocation(usize),            // attempted to allocate a huge chunk of memory with alloca
}

pub type Result<T> = core::result::Result<T, PacketHandlingError>;

impl From<globed_shared::anyhow::Error> for PacketHandlingError {
    fn from(value: globed_shared::anyhow::Error) -> Self {
        Self::Other(value.to_string())
    }
}

impl From<reqwest::Error> for PacketHandlingError {
    fn from(value: reqwest::Error) -> Self {
        Self::WebRequestError(value)
    }
}

impl From<SystemTimeError> for PacketHandlingError {
    fn from(value: SystemTimeError) -> Self {
        Self::SystemTimeError(value)
    }
}

impl From<std::io::Error> for PacketHandlingError {
    fn from(value: std::io::Error) -> Self {
        Self::IOError(value)
    }
}

impl From<ColorParseError> for PacketHandlingError {
    fn from(value: ColorParseError) -> Self {
        Self::ColorParseFailed(value)
    }
}

impl From<DecodeError> for PacketHandlingError {
    fn from(value: DecodeError) -> Self {
        Self::MalformedPacketStructure(value)
    }
}

impl Display for PacketHandlingError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Other(msg) => f.write_str(msg),
            Self::IOError(msg) => f.write_fmt(format_args!("IO Error: {msg}")),
            Self::WrongCryptoBoxState => f.write_str("wrong crypto box state for the given operation"),
            Self::EncryptionError => f.write_str("Encryption failed"),
            Self::DecryptionError => f.write_str("Decryption failed"),
            Self::MalformedCiphertext => f.write_str("malformed ciphertext in an encrypted packet"),
            Self::MalformedMessage => f.write_str("malformed message structure"),
            Self::MalformedLoginAttempt => f.write_str("malformed login attempt"),
            Self::MalformedPacketStructure(err) => f.write_fmt(format_args!("could not decode a packet: {err}")),
            Self::NoHandler(id) => f.write_fmt(format_args!("no packet handler for packet ID {id}")),
            Self::WebRequestError(msg) => f.write_fmt(format_args!("web request error: {msg}")),
            Self::UnexpectedPlayerData => {
                f.write_str("received PlayerDataPacket or SyncPlayerMetadataPacket when not in a level")
            }
            Self::SystemTimeError(msg) => f.write_fmt(format_args!("system time error: {msg}")),
            Self::SocketSendFailed(err) => f.write_fmt(format_args!("socket send failed: {err}")),
            Self::SocketWouldBlock => f.write_str("could not do a non-blocking operation on the socket as it would block"),
            Self::UnexpectedCentralResponse => f.write_str("got unexpected response from the central server"),
            Self::ColorParseFailed(err) => f.write_fmt(format_args!("failed to parse a color: {err}")),
            Self::Ratelimited => f.write_str("client is sending way too many packets per second"),
            Self::DangerousAllocation(size) => f.write_fmt(format_args!(
                "attempted to allocate {size} bytes on the stack - that has a potential for a stack overflow!"
            )),
        }
    }
}
