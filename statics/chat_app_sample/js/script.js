var chat_screen = null;
var chat_input = null;
var send_button = null;
var ws = null;
var authorid = null;
var cookie_info = {};

const request_history = () => {
  if (ws === null || authorid === null) {
    return;
  }
};

const update_chat_screen = () => {};

const add_chat_log = (__message, __authorid, __time) => {
  if (chat_screen === null || chat_screen === undefined) {
    return;
  }
  let d = new Date();
  d.setTime(__time);
  console.log(d);
  let isSelf = __authorid === authorid;
  let chat_panel_area = document.createElement("div");
  chat_panel_area.classList.add("chat_panel_area");
  let blank = document.createElement("div");
  blank.classList.add("blank");
  let chat_info = document.createElement("div");
  chat_info.classList.add("chat_info");
  let chat_author = document.createElement("div");
  chat_author.classList.add("chat_author");
  chat_author.innerText = "Me";
  let post_time = document.createElement("div");
  post_time.classList.add("post_time");
  post_time.innerText = d.getHours() + ":" + d.getMinutes();
  let chat_panel = document.createElement("div");
  chat_panel.innerText = __message;
  chat_panel.classList.add(isSelf ? "chat_panel_self" : "chat_panel");
  chat_info.appendChild(chat_author);
  chat_info.appendChild(post_time);
  if (isSelf) {
    chat_panel_area.appendChild(blank);
    chat_panel_area.appendChild(chat_info);
    chat_panel_area.appendChild(chat_panel);
  } else {
    chat_panel_area.appendChild(chat_info);
    chat_panel_area.appendChild(chat_panel);
    chat_panel_area.appendChild(blank);
  }
  chat_screen.appendChild(chat_panel_area);
  setTimeout(() => {
    chat_screen.scrollTop = chat_screen.scrollHeight;
  });
};

const get_user_info = () => {};

const set_cookie = (cname, cvalue, exdays) => {
  const d = new Date();
  d.setTime(d.getTime() + exdays * 24 * 60 * 60 * 1000);
  let expires = "expires=" + d.toUTCString();
  document.cookie = cname + "=" + cvalue + ";" + expires + ";path=/";
};

const get_cookie = (cname) => {
  let name = cname + "=";
  let ret = undefined;
  document.cookie
    .split(";")
    .map((item) => item.trim())
    .forEach((item) => {
      if (item.startsWith(name)) {
        ret = item.split("=")[1];
      }
    });
  return ret;
};

const connectWs = (initialize = false) => {
  ws = new WebSocket(`ws://${window.location.host}`);
  ws.onopen = (event) => {
    console.log("onopen", event);
    if (initialize) {
      if (authorid === undefined) {
        // userid cannot be found from cookie => request userid
        ws.send(
          JSON.stringify({
            cmd: "request",
            args: ["userid"],
          })
        );
      } else {
        ws.send(
          JSON.stringify({
            cmd: "request",
            args: ["set_userid", authorid],
          })
        );
        // else => restore uuid from cookie
        // set uuid as author id
        // authorid = cookie["uuid"];
        request_history();
      }
    }

    // request history to backend
  };

  ws.onmessage = (event) => {
    let info = JSON.parse(event.data);
    console.log("onmessage", info);
    if (info.subject === undefined || info.subject === null) {
      return;
    } else if (info.subject === "userid") {
      authorid = info.authorid;
      set_cookie("authorid", authorid, 7);
    } else if (info.subject === "set_userid") {
      console.log(info);
    } else if (info.subject === "history") {
      update_chat_screen();
    } else if (info.subject === "userinfo") {
      get_user_info();
    } else if (info.subject === "onMessage") {
      add_chat_log(info.message, info.authorid, info.time);
    }
  };

  ws.onclose = (event) => {
    console.log("onclose", event.data);
    connectWs(false);
    // show error message?
  };
};

window.onload = () => {
  chat_screen = document.getElementById("chat_screen");
  chat_input = document.getElementById("chat_input");
  send_button = document.getElementById("send_button");

  authorid = get_cookie("authorid");

  connectWs(true);
  console.log("onload");
  console.log(chat_screen);
  console.log(chat_input.value);
  console.log(send_button);

  send_button.onclick = (event) => {
    send_chat_message();
  };

  chat_input.onkeydown = (event) => {
    if (event.key === "Enter") {
      send_chat_message();
    }
  };

  const send_chat_message = () => {
    if (ws === null || chat_input === null) {
      return;
    }
    if (chat_input.value === null || chat_input.value.trim() === "") {
      return;
    }
    // send message
    console.log("send", chat_input.value);
    // clear form value
    let unixtime = new Date().getTime();
    ws.send(
      JSON.stringify({
        cmd: "msg",
        args: [chat_input.value],
        time: unixtime,
      })
    );
    chat_input.value = "";
  };
};
