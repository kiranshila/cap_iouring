use itertools::Itertools;
use socket2::{Protocol, Socket, Type};
use std::net::SocketAddr;
use tokio_uring::net::UdpSocket;

/// Size of the packet count header
const TIMESTAMP_SIZE: usize = 8;
/// Total number of bytes in the spectra block of the UDP payload
const SPECTRA_SIZE: usize = 8192;
/// Total UDP payload size
pub const PAYLOAD_SIZE: usize = SPECTRA_SIZE + TIMESTAMP_SIZE;
const CAP_PACKS: usize = 10_000_000;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    tokio_uring::start(async {
        let local: SocketAddr = "0.0.0.0:60000".parse().unwrap();

        let sock = Socket::new(socket2::Domain::IPV4, Type::DGRAM, Some(Protocol::UDP))?;
        sock.set_reuse_port(true)?;
        sock.set_reuse_address(true)?;
        sock.set_nonblocking(true)?;
        sock.bind(&local.into())?;
        sock.set_recv_buffer_size(2 * 256 * 1024 * 1024)?; // 256 MB (like Vikram)

        let sock = UdpSocket::from_std(sock.into());
        let mut counts = vec![0u64; CAP_PACKS];
        let mut packets = 0usize;

        while packets < CAP_PACKS {
            let buf = vec![0; PAYLOAD_SIZE];
            let (result, buf) = sock.recv_from(buf).await;
            if let Ok((n, _)) = result {
                if n == PAYLOAD_SIZE {
                    counts[packets] =
                        u64::from_be_bytes(buf[0..TIMESTAMP_SIZE].try_into().unwrap());
                    packets += 1;
                } else {
                    eprintln!("Corrupt packet");
                    continue;
                }
            } else {
                eprintln!("Bad recv");
                continue;
            }
        }
        counts.sort();
        let mut deltas: Vec<_> = counts.windows(2).map(|x| x[1] - x[0]).collect();
        deltas.sort();
        let deltas: Vec<_> = deltas.iter().dedup_with_count().collect();
        dbg!(deltas);
        Ok(())
    })
}
