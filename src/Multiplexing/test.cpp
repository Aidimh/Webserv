// ============================================================================
// Multiplexer::_writeClient - refactored into small, single-purpose helpers.
// Behavior is identical to the original, only readability changed.
// C++98 only.
//
// NOTE: add these private method declarations to Multiplexer.hpp:
//
//   enum SendResult { SEND_ERROR, SEND_INCOMPLETE, SEND_COMPLETE };
//
//   void       _prepareResponseIfNeeded(Client &client);
//   SendResult _sendBufferedResponse(int fd, Client &client);
//   void       _handleStreaming(int fd, Client &client);
//   bool       _refillStreamBuffer(Client &client);
//   void       _sendStreamChunk(int fd, Client &client);
//   void       _endStream(int fd, Client &client);
//   void       _finishResponse(int fd, Client &client);
//   void       _clearPollout(int fd);
// ============================================================================

// ----------------------------------------------------------------------
// Builds the response once and caches it on the client.
// ----------------------------------------------------------------------
void Multiplexer::_prepareResponseIfNeeded(Client &client)
{
    if (client.response_prepared)
        return;
`
    Response response = Dispatcher::dispatch(client, which_server(client.port));
    client.response = response.toString();
    client.response_prepared = true;
}

// ----------------------------------------------------------------------
// Sends whatever is left in client.response.
// Returns SEND_ERROR       -> send() failed, caller must remove the client
// Returns SEND_INCOMPLETE  -> some bytes still queued, caller must wait
// Returns SEND_COMPLETE    -> buffer fully flushed (or was already empty)
// ----------------------------------------------------------------------
Multiplexer::SendResult Multiplexer::_sendBufferedResponse(int fd, Client &client)
{
    if (client.response.empty())
        return SEND_COMPLETE;

    ssize_t sent = send(fd, client.response.c_str(), client.response.size(), MSG_NOSIGNAL);
    if (sent <= 0)
        return SEND_ERROR;

    client.response.erase(0, sent);
    return client.response.empty() ? SEND_COMPLETE : SEND_INCOMPLETE;
}

// ----------------------------------------------------------------------
// Reads the next chunk from the streamed file into the client buffer.
// Returns false when there is nothing left to read (stream is finished).
// ----------------------------------------------------------------------
bool Multiplexer::_refillStreamBuffer(Client &client)
{
    client.stream_buffer_size = read(client.stream_file_fd, client.stream_buffer, sizeof(client.stream_buffer));
    client.stream_buffer_offset = 0;
    return client.stream_buffer_size > 0;
}

// ----------------------------------------------------------------------
// Sends the current chunk of the streamed file.
// On failure, closes the file and removes the client.
// ----------------------------------------------------------------------
void Multiplexer::_sendStreamChunk(int fd, Client &client)
{
    ssize_t sent = send(
        fd,
        client.stream_buffer + client.stream_buffer_offset,
        client.stream_buffer_size - client.stream_buffer_offset,
        MSG_NOSIGNAL
    );

    if (sent <= 0)
    {
        close(client.stream_file_fd);
        _removeClient(fd);
        return;
    }

    client.stream_buffer_offset += sent;
    client.stream_bytes_remaining -= sent;
}

// ----------------------------------------------------------------------
// Closes the streamed file and resets all stream-related state.
// ----------------------------------------------------------------------
void Multiplexer::_endStream(int fd, Client &client)
{
    close(client.stream_file_fd);

    client.stream_file_fd = -1;
    client.stream_bytes_remaining = 0;
    client.response_prepared = false;

    _clearPollout(fd);
}

// ----------------------------------------------------------------------
// Orchestrates one streaming step: refill buffer if needed, then send.
// ----------------------------------------------------------------------
void Multiplexer::_handleStreaming(int fd, Client &client)
{
    bool bufferNeedsRefill = (client.stream_buffer_offset == client.stream_buffer_size);

    if (bufferNeedsRefill)
    {
        if (!_refillStreamBuffer(client))
        {
            _endStream(fd, client);
            return;
        }
    }

    _sendStreamChunk(fd, client);
}

// ----------------------------------------------------------------------
// Marks the (non-streamed) response as fully finished for this client.
// ----------------------------------------------------------------------
void Multiplexer::_finishResponse(int fd, Client &client)
{
    client.response_prepared = false;
    _clearPollout(fd);
}

// ----------------------------------------------------------------------
// Removes POLLOUT from the pollfd entry matching fd.
// ----------------------------------------------------------------------
void Multiplexer::_clearPollout(int fd)
{
    for (size_t i = 0; i < _pollfds.size(); i++)
    {
        if (_pollfds[i].fd == fd)
        {
            _pollfds[i].events &= ~POLLOUT;
            break;
        }
    }
}

// ============================================================================
// Orchestrator: reads top-to-bottom like a checklist.
// ============================================================================
void Multiplexer::_writeClient(int fd)
{
    std::map<int, Client>::iterator it = _clients.find(fd);
    if (it == _clients.end())
        return;

    Client &client = it->second;

    _prepareResponseIfNeeded(client);

    SendResult result = _sendBufferedResponse(fd, client);
    if (result == SEND_ERROR)
    {
        _removeClient(fd);
        return;
    }
    if (result == SEND_INCOMPLETE)
        return;

    if (client.stream_file_fd != -1)
    {
        _handleStreaming(fd, client);
        return;
    }

    _finishResponse(fd, client);
}
